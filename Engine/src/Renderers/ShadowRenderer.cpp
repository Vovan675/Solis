#include "pch.h"
#include "ShadowRenderer.h"
#include "Rendering/Renderer.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Rendering/SceneRenderer.h"
#include "FrameGraph/FrameGraph.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphUtils.h"
#include "glm/glm.hpp"
#include "Editor/EditorContext.h"
#include "Utils/Camera.h"
#include "Core/Variables.h"
#include "Rendering/GlobalBufferCache.h"

ShadowRenderer::ShadowRenderer()
{
	if (engine_ray_tracing)
	{
		raygen_shader = gDynamicRHI->createShader(L"shaders/rt/rt_shader.hlsl", RAY_GENERATION_SHADER);
		miss_shader = gDynamicRHI->createShader(L"shaders/rt/rt_shader.hlsl", MISS_SHADER);

		closest_hit_shader = gDynamicRHI->createShader(L"shaders/rt/rt_shader.hlsl", CLOSEST_HIT_SHADER);
		TextureDescription tex_description = {};
		tex_description.width = gDynamicRHI->getSwapchainTexture(0)->getWidth();
		tex_description.height = gDynamicRHI->getSwapchainTexture(0)->getHeight();
		tex_description.format = FORMAT_R8G8B8A8_UNORM;
		tex_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC | TEXTURE_USAGE_STORAGE;
		storage_image = gDynamicRHI->createTexture(tex_description);
		storage_image->fill();
	}
}

void ShadowRenderer::addShadowMapPasses(FrameGraph &fg, uint32_t max_draw_calls_count)
{
	OpaqueGeometryPass opaque;

	auto light_entities_id = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();

	ShadowPasses &shadow_passes = fg.getBlackboard().add<ShadowPasses>();

	uint32_t shadow_view_id = 1; // TODO: in future registrate frustums in separate system and use real view_id (view_id is tied to pass_mask and view_projection)

	for (entt::entity light_entity_id : light_entities_id)
	{
		Entity light_entity(light_entity_id);
		auto &light = light_entity.getComponent<LightComponent>();

		glm::vec3 scale, position, skew;
		glm::vec4 persp;
		glm::quat rotation;
		glm::decompose(light_entity.getWorldTransformMatrix(), scale, rotation, position, skew, persp);

		if (light.getType() == LIGHT_TYPE_POINT)
		{
			glm::mat4 faces_transforms[6] = {
				glm::lookAtLH(position, position + glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)),
				glm::lookAtLH(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0)),
				glm::lookAtLH(position, position + glm::vec3(0, 1, 0), glm::vec3(0, 0, -1)),
				glm::lookAtLH(position, position + glm::vec3(0, -1, 0), glm::vec3(0, 0, 1)),
				glm::lookAtLH(position, position + glm::vec3(0, 0, 1), glm::vec3(0, 1, 0)),
				glm::lookAtLH(position, position + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)),
			};
			glm::mat4 light_projection = glm::perspectiveLH(glm::radians(90.0f), 1.0f, POINT_SHADOW_Z_NEAR, light.radius);

			GraphicsResourceName shadow_map_resource = GFXRID_ID(ShadowMap, (uint32_t)light_entity_id);
			fg.importTexture(shadow_map_resource, light.shadow_map);
			shadow_passes.shadow_maps.push_back(shadow_map_resource);

			OpaqueGeometryPass::ShaderSet shaders = OpaqueGeometryPass::ShaderSet::fromFile(L"shaders/lighting/shadows.hlsl");

			for (int face = 0; face < 6; face++)
			{
				OpaqueGeometryPass::RenderView view;
				view.view_projection = light_projection * faces_transforms[face];
				view.pass_mask = PASS_MASK_POINT_SHADOW;
				view.instance_count = max_draw_calls_count;
				view.view_id = shadow_view_id++;
				view.render_size = glm::ivec2(light.shadow_map->getWidth());
				view.layer = face;
				view.use_two_pass_occlusion = false;
				view.use_reverse_z = false;
				view.cull_mode = CULL_MODE_FRONT;
				view.shaders = shaders;

				OpaqueGeometryPass::DepthOutput output;
				output.depth = shadow_map_resource;

				opaque.renderDepth(fg, view, output);
			}
		} else
		{
			GraphicsResourceName shadow_map_resource = GFXRID_ID(ShadowMap, (uint32_t)light_entity_id);
			fg.importTexture(shadow_map_resource, light.shadow_map);
			shadow_passes.shadow_maps.push_back(shadow_map_resource);

			uint32_t shadow_size = light.shadow_map->getWidth();
			HiZ::createOrImport(fg, cascade_hiz, GFXRID(CascadeHiZ), glm::ivec2(shadow_size / 4), SHADOW_MAP_CASCADE_COUNT);

			OpaqueGeometryPass::ShaderSet shaders = OpaqueGeometryPass::ShaderSet::fromFile(L"shaders/lighting/shadows.hlsl");

			for (int cascade = 0; cascade < SHADOW_MAP_CASCADE_COUNT; cascade++)
			{
				OpaqueGeometryPass::RenderView view;
				view.view_projection = light.cascades[cascade].viewProjMatrix;
				view.pass_mask = PASS_MASK_DIRECTIONAL_SHADOW;
				view.instance_count = max_draw_calls_count;
				view.view_id = shadow_view_id++;
				view.render_size = glm::ivec2(shadow_size);
				view.hiz = GFXRID(CascadeHiZ);
				view.layer = cascade;
				view.use_two_pass_occlusion = true;
				view.ortho_frustum = true;
				view.use_reverse_z = false;
				view.cull_mode = CULL_MODE_FRONT;
				view.shaders = shaders;

				OpaqueGeometryPass::DepthOutput output;
				output.depth = shadow_map_resource;

				opaque.renderDepth(fg, view, output);
			}
		}
	}
}

void ShadowRenderer::addRayTracedShadowPasses(FrameGraph & fg, Ref<RayTracingScene> rt_scene)
{
	if (!rt_scene || !rt_scene->getTopLevelAS())
		return;

	fg.addCallbackPass("Ray Traced Shadows Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(GFXRID(RayTracedVisibility), Renderer::getRenderWidth(), Renderer::getRenderHeight(), gDynamicRHI->getSwapchainTexture(0)->getDescription().format);
		builder.writeUAVTexture(GFXRID(RayTracedVisibility)); // was transfer

		builder.readTexture(GFXRID(GBufferDepth));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto visiblity = resources.getTexture(GFXRID(RayTracedVisibility));

		gGlobalPipeline->setupRayTracing(raygen_shader, miss_shader, closest_hit_shader);
		gGlobalPipeline->flushAndBind(cmd_list);

		struct LightUBO
		{
			glm::vec4 dir_light_direction;
			uint32_t depth_texture_id;
			uint32_t output_texture_id;
		} ubo_light;

		auto lights = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();
		for (auto entity_id : lights)
		{
			Entity light_entity(entity_id);
			if (light_entity.getComponent<LightComponent>().getType() == LIGHT_TYPE_DIRECTIONAL)
			{
				glm::vec3 scale, position, skew;
				glm::vec4 persp;
				glm::quat rotation;
				glm::decompose(light_entity.getWorldTransformMatrix(), scale, rotation, position, skew, persp);

				ubo_light.dir_light_direction = rotation * glm::vec4(0, 0, -1, 1);
				break;
			}
		}

		ubo_light.depth_texture_id = resources.getReadTexture(GFXRID(GBufferDepth));

		auto ray_traced_lighting = visiblity;
		ubo_light.output_texture_id = ray_traced_lighting->getUnorderedAccessView()->getBindlessIndex();

		gDynamicRHI->setConstantBufferData(1, &ubo_light, sizeof(LightUBO));

		cmd_list->dispatchRays(Renderer::getRenderResolution().x, Renderer::getRenderResolution().y, 1);

	});
}

void ShadowRenderer::updateShadows(Camera *camera)
{
	auto components = Scene::getCurrentScene()->getEntitiesWith<TransformComponent, LightComponent>();

	for (auto &&[entity, transform, light] : components.each())
	{
		if (light.getType() == LIGHT_TYPE_DIRECTIONAL)
		{
			glm::vec3 scale, position, skew;
			glm::vec4 persp;
			glm::quat rotation;
			glm::decompose(transform.getWorldTransform(), scale, rotation, position, skew, persp);

			glm::vec3 light_dir = transform.getLocalDirection(glm::vec3(0, 0, 1));
			debug_renderer->addArrow(position, position + light_dir, 0.1);
			update_cascades(light, light_dir, camera);
		} else if (light.getType() == LIGHT_TYPE_POINT)
		{
			glm::vec3 position = glm::vec3(transform.getWorldTransform()[3]);
			debug_renderer->addSphere(position, POINT_SHADOW_Z_NEAR, 16, glm::vec3(1, 0.4, 0));
			debug_renderer->addSphere(position, light.radius, 32, glm::vec3(1, 1, 0));
		}
	}
}

void ShadowRenderer::update_cascades(LightComponent &light, glm::vec3 light_dir, Camera *camera)
{
	float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

	float nearClip = std::min(camera->getNear(), camera->getFar());
	float farClip = std::max(camera->getFar(), camera->getNear());
	farClip = std::min(farClip, (float)3000.0f);


	glm::mat4 camera_projections[SHADOW_MAP_CASCADE_COUNT];


	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	// Calculate split depths based on view camera frustum
	// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
	{
		float fi = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
		float l = nearClip * pow(farClip / nearClip, fi);
		float u = nearClip + (farClip - nearClip) * fi;
		cascadeSplits[i] = l * 0.95 + u * (1.0f - 0.95);
	}

	camera_projections[0] = glm::perspective(glm::radians(camera->getFov()), camera->getAspect(), nearClip, cascadeSplits[0]); // Not Swapped intentionally, reconstruct non-reverse-z projection
	for (uint32_t i = 1; i < SHADOW_MAP_CASCADE_COUNT; i++)
	{
		camera_projections[i] = glm::perspective(glm::radians(camera->getFov()), camera->getAspect(), cascadeSplits[i - 1], cascadeSplits[i]);
	}
	
	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
	{
		float splitDist = cascadeSplits[i];

		glm::vec3 frustumCorners[8] = {
			glm::vec3(-1.0f, 1.0f, 1.0f),
			glm::vec3(1.0f, 1.0f, 1.0f),
			glm::vec3(1.0f, -1.0f, 1.0f),
			glm::vec3(-1.0f, -1.0f, 1.0f),

			glm::vec3(-1.0f, 1.0f,  0.0f),
			glm::vec3(1.0f, 1.0f,  0.0f),
			glm::vec3(1.0f, -1.0f,  0.0f),
			glm::vec3(-1.0f, -1.0f,  0.0f),
		};

		// Project frustum corners into world space
		glm::mat4 invCam = glm::inverse(camera_projections[i] * camera->getView());
		for (uint32_t j = 0; j < 8; j++)
		{
			glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
			frustumCorners[j] = invCorner / invCorner.w;
		}

		// Get frustum center
		glm::vec3 frustumCenter = glm::vec3(0.0f);
		for (uint32_t j = 0; j < 8; j++)
		{
			frustumCenter += frustumCorners[j];
		}
		frustumCenter /= 8.0f;

		float radius = 0.0f;
		for (uint32_t j = 0; j < 8; j++)
		{
			float distance = glm::length(frustumCorners[j] - frustumCenter);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 8.0f) / 8.0f;

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

		glm::mat4 lightViewMatrix = glm::lookAtRH(frustumCenter, frustumCenter + light_dir * radius, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightOrthoMatrix = glm::orthoRH(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, minExtents.z - 1.2f * radius, maxExtents.z * 1.2f);


		/*
		glm::vec3 color;
		if (i == 0)
			color = glm::vec3(1, 0, 0);
		else if (i == 1)
			color = glm::vec3(0, 1, 0);
		else if (i == 2)
			color = glm::vec3(1, 1, 0);
		else if (i == 3)
			color = glm::vec3(1, 1, 1);
		debug_renderer->addFrustum(glm::inverse(camera_projections[i]), color);
		*/


		// Fix shimmering
		float shadow_map_size = 4096;
		glm::vec2 shadow_origin = (lightOrthoMatrix * lightViewMatrix * glm::vec4(0, 0, 0, 1)) * shadow_map_size / 2.0f;
		glm::vec2 round_offset = glm::round(shadow_origin) - shadow_origin;
		round_offset = round_offset * 2.0f / shadow_map_size;
		lightOrthoMatrix[3] += glm::vec4(round_offset, 0, 0);

		// Store split distance and matrix in cascade
		light.cascades[i].splitDepth = splitDist;
		light.cascades[i].viewProjMatrix = lightOrthoMatrix * lightViewMatrix;

		lastSplitDist = cascadeSplits[i];
	}
}
