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
		raygen_shader = gDynamicRHI->createShader(L"shaders/rt/rt_shader.hlsl", ShaderType::RAY_GENERATION);
		miss_shader = gDynamicRHI->createShader(L"shaders/rt/rt_shader.hlsl", ShaderType::MISS);

		closest_hit_shader = gDynamicRHI->createShader(L"shaders/rt/rt_shader.hlsl", ShaderType::CLOSEST_HIT);
	}
}

void ShadowRenderer::addCullViews(GpuTable<CullViewGPU> &cull_views, Camera *camera)
{
	shadow_views.clear();

	auto components = Scene::getCurrentScene()->getEntitiesWith<TransformComponent, LightComponent>();

	for (auto &&[entity, transform, light] : components.each())
	{
		glm::vec3 position = glm::vec3(transform.getWorldTransform()[3]);

		ShadowViews shadow{};
		shadow.light_entity = entity;
		shadow.shadow_map = light.getShadowMap();
		shadow.type = light.getType();

		if (light.getType() == LIGHT_TYPE_DIRECTIONAL)
		{
			glm::vec3 light_dir = transform.getLocalDirection(glm::vec3(0, 0, 1));
			update_cascades(light, light_dir, camera);

			if (Renderer::isRayTracedShadowsEnabled())
				continue;

			CullViewGPU cascade_views[SHADOW_MAP_CASCADE_COUNT]{};
			for (int cascade = 0; cascade < SHADOW_MAP_CASCADE_COUNT; cascade++)
			{
				cascade_views[cascade].view_projection = light.getCascadeViewProj(cascade);
				cascade_views[cascade].pass_mask = PASS_MASK_DIRECTIONAL_SHADOW;
				cascade_views[cascade].is_ortho = true;
				cascade_views[cascade].near_clip = false;
			}
			shadow.first_view_id = cull_views.addArray({cascade_views, SHADOW_MAP_CASCADE_COUNT});
		} else
		{
			//debug_renderer->addSphere(position, POINT_SHADOW_Z_NEAR, 16, glm::vec3(1, 0.4, 0));
			//debug_renderer->addSphere(position, light.attenuation_radius, 32, glm::vec3(1, 1, 0));

			glm::mat4 faces_transforms[6] = {
				glm::lookAtLH(position, position + glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)),
				glm::lookAtLH(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0)),
				glm::lookAtLH(position, position + glm::vec3(0, 1, 0), glm::vec3(0, 0, -1)),
				glm::lookAtLH(position, position + glm::vec3(0, -1, 0), glm::vec3(0, 0, 1)),
				glm::lookAtLH(position, position + glm::vec3(0, 0, 1), glm::vec3(0, 1, 0)),
				glm::lookAtLH(position, position + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)),
			};
			glm::mat4 light_projection = glm::perspectiveLH(glm::radians(90.0f), 1.0f, light.attenuation_radius, POINT_SHADOW_Z_NEAR); // Swapped intentionally, Inverse-Z

			CullViewGPU face_views[6]{};
			for (int face = 0; face < 6; face++)
			{
				face_views[face].view_projection = light_projection * faces_transforms[face];
				face_views[face].pass_mask = PASS_MASK_POINT_SHADOW;
			}
			shadow.first_view_id = cull_views.addArray({face_views, 6});
		}

		shadow_views.push_back(shadow);
	}
}

void ShadowRenderer::addShadowMapPasses(FrameGraph &fg, uint32_t max_draw_calls_count)
{
	OpaqueGeometryPass opaque;
	OpaqueGeometryPass::ShaderSet shaders = OpaqueGeometryPass::ShaderSet::fromFile(L"shaders/lighting/shadows.hlsl");

	ShadowPasses &shadow_passes = fg.getBlackboard().add<ShadowPasses>();

	for (const ShadowViews &shadow : shadow_views)
	{
		GraphicsResourceName shadow_map_resource = GFXRID_ID(ShadowMap, (uint32_t)shadow.light_entity);
		fg.importTexture(shadow_map_resource, shadow.shadow_map);
		shadow_passes.shadow_maps.push_back(shadow_map_resource);

		uint32_t shadow_size = shadow.shadow_map->getWidth();

		OpaqueGeometryPass::DepthOutput output;
		output.depth = shadow_map_resource;

		if (shadow.type == LIGHT_TYPE_POINT)
		{
			for (int face = 0; face < 6; face++)
			{
				OpaqueGeometryPass::RenderView view;
				view.view_id = shadow.first_view_id + face;
				view.pass_mask = PASS_MASK_POINT_SHADOW;
				view.instance_count = max_draw_calls_count;
				view.render_size = glm::ivec2(shadow_size);
				view.layer = face;
				view.use_two_pass_occlusion = false;
				view.cull_mode = CullMode::FRONT;
				view.shaders = shaders;

				opaque.renderDepth(fg, view, output);
			}
		} else
		{
			TextureDescription hiz_description = HiZ::createDescription(glm::ivec2(shadow_size / 4), SHADOW_MAP_CASCADE_COUNT);

			for (int cascade = 0; cascade < SHADOW_MAP_CASCADE_COUNT; cascade++)
			{
				OpaqueGeometryPass::RenderView view;
				view.view_id = shadow.first_view_id + cascade;
				view.pass_mask = PASS_MASK_DIRECTIONAL_SHADOW;
				view.instance_count = max_draw_calls_count;
				view.render_size = glm::ivec2(shadow_size);
				view.hiz = GFXRID_ID(CascadeHiZ, (uint32_t)shadow.light_entity);
				view.hiz_description = hiz_description;
				view.layer = cascade;
				view.use_two_pass_occlusion = true;
				view.use_reverse_z = false;
				view.depth_clip = false;
				view.cull_mode = CullMode::FRONT;
				view.shaders = shaders;

				opaque.renderDepth(fg, view, output);
			}
		}
	}
}

void ShadowRenderer::addRayTracedShadowPasses(FrameGraph & fg, Ref<RayTracingScene> rt_scene, uint32_t sun_light_index)
{
	if (!rt_scene || !rt_scene->getTopLevelAS() || sun_light_index == INVALID_LIGHT_INDEX)
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

		struct Constants
		{
			uint32_t light_index;
			uint32_t depth_texture_id;
			uint32_t output_texture_id;
		} constants;
		constants.light_index = sun_light_index;
		constants.depth_texture_id = resources.getReadTexture(GFXRID(GBufferDepth));
		constants.output_texture_id = visiblity->getUnorderedAccessView()->getBindlessIndex();
		gDynamicRHI->setConstantBufferData(1, &constants, sizeof(constants));

		cmd_list->dispatchRays(Renderer::getRenderResolution().x, Renderer::getRenderResolution().y, 1);

	});
}

void ShadowRenderer::update_cascades(LightComponent &light, glm::vec3 light_dir, Camera *camera)
{
	float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

	float nearClip = std::min(camera->getNear(), camera->getFar());
	float farClip = std::max(camera->getFar(), camera->getNear());
	farClip = std::min(farClip, GFXOPTIONS(shadows).cascades_distance);


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
		glm::vec2 shadow_origin = (lightOrthoMatrix * lightViewMatrix * glm::vec4(0, 0, 0, 1)) * (float)light.shadow_map_size / 2.0f;
		glm::vec2 round_offset = glm::round(shadow_origin) - shadow_origin;
		round_offset = round_offset * 2.0f / (float)light.shadow_map_size;
		lightOrthoMatrix[3] += glm::vec4(round_offset, 0, 0);

		// Store split distance and matrix in cascade
		light.cascades[i].splitDepth = splitDist;
		light.cascades[i].viewProjMatrix = lightOrthoMatrix * lightViewMatrix;

		lastSplitDist = cascadeSplits[i];
	}
}
