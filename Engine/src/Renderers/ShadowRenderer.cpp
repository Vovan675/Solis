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
#include "EntityRenderer.h"
#include "glm/glm.hpp"
#include "Editor/EditorContext.h"
#include "Utils/Camera.h"
#include "Core/Variables.h"
#include <RHI/Vulkan/VulkanDynamicRHI.h>

ShadowRenderer::ShadowRenderer()
{
	shadows_vertex_shader = gDynamicRHI->createShader(L"shaders/lighting/shadows.hlsl", VERTEX_SHADER);
	shadows_fragment_shader_point = gDynamicRHI->createShader(L"shaders/lighting/shadows.hlsl", FRAGMENT_SHADER, {{"LIGHT_TYPE", "0"}});
	shadows_fragment_shader_directional = gDynamicRHI->createShader(L"shaders/lighting/shadows.hlsl", FRAGMENT_SHADER, {{"LIGHT_TYPE", "1"}});

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

void ShadowRenderer::addShadowMapPasses(FrameGraph &fg, const eastl::vector<RenderBatch> &batches)
{
	auto light_entities_id = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();

	ShadowPasses &shadow_passes = fg.getBlackboard().add<ShadowPasses>();

	struct ShadowPassData
	{
		FrameGraphTextureId shadow_map;
	};

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
			eastl::vector<glm::mat4> faces_transforms;
			faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)));
			faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0)));
			faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)));
			faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, -1, 0), glm::vec3(0, 0, 1)));
			faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, 0, 1), glm::vec3(0, 1, 0)));
			faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)));


			FrameGraphTextureId shadow_map_resource = fg.importTexture(GraphicsResourceName((eastl::string("Shadow Map ") + eastl::to_string((uint32_t)light_entity_id)).c_str()), light.shadow_map);

			fg.addCallbackPass<ShadowPassData>("Cube Shadow Map Pass",
			[&](RenderPassBuilder &builder, ShadowPassData &data)
			{
				data.shadow_map = builder.writeTexture(shadow_map_resource);
				shadow_passes.shadow_maps.push_back(data.shadow_map);
			},
		[=, &batches](const ShadowPassData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
		{
			// Render
			auto shadow_map = resources.getTexture(data.shadow_map);

				for (int face = 0; face < 6; face++)
				{
					if (faces_transforms.size() <= face)
						continue;

					glm::mat4 light_projection = glm::perspectiveLH(glm::radians(90.0f), 1.0f, 0.05f, light.radius);
					glm::mat4 light_matrix = light_projection * faces_transforms[face];

					cmd_list->setRenderTargets({}, shadow_map, face, 0, true);

					auto &p = gGlobalPipeline;
					p->setupGraphicsPipeline(cmd_list, shadows_vertex_shader, shadows_fragment_shader_point,
											 VertexInputsDescription{}, false, true, CULL_MODE_FRONT);
					p->flushAndBind(cmd_list);

					EntityRenderer::ShadowUBO ubo;
					ubo.light_space_matrix = light_matrix;
					ubo.light_pos = glm::vec4(position, 1.0);
					ubo.z_far = light.radius;

					gDynamicRHI->setConstantBufferData(1, &ubo, sizeof(EntityRenderer::ShadowUBO));

					BoundFrustum bound_frustum(light_projection, faces_transforms[face]);
					for (const RenderBatch &batch : batches)
					{
						if (!batch.world_bound_box.isInside(bound_frustum))
							continue;

						struct ShadowPushConstact
						{
							uint32_t instance_id;
						} push_constant;

						push_constant.instance_id = batch.instance_id;
						gDynamicRHI->setConstantBufferData(2, &push_constant, sizeof(ShadowPushConstact));

						cmd_list->setIndexBuffer(batch.mesh->indexBuffer);
						cmd_list->drawIndexedInstanced(batch.mesh->indices.size(), 1, 0, 0, 0);
						Renderer::addDrawCalls(1);
					}

					cmd_list->resetRenderTargets();
				}
			});
		} else
		{
			FrameGraphTextureId shadow_map_resource = fg.importTexture(GraphicsResourceName((eastl::string("Shadow Map ") + eastl::to_string((uint32_t)light_entity_id)).c_str()), light.shadow_map);

			fg.addCallbackPass<ShadowPassData>("Cascaded Shadows Pass",
			[&](RenderPassBuilder &builder, ShadowPassData &data)
			{
				data.shadow_map = builder.writeTexture(shadow_map_resource);
				shadow_passes.shadow_maps.push_back(data.shadow_map);
			},
		[=, &batches, &light](const ShadowPassData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
		{
			// Render
			auto shadow_map = resources.getTexture(data.shadow_map);

				for (int c = 0; c < SHADOW_MAP_CASCADE_COUNT; c++)
				{
					glm::mat4 light_matrix = light.cascades[c].viewProjMatrix;

					cmd_list->setRenderTargets({}, shadow_map, c, 0, true);

					auto &p = gGlobalPipeline;
					p->setupGraphicsPipeline(cmd_list, shadows_vertex_shader, shadows_fragment_shader_directional,
											 VertexInputsDescription{}, false, true, CULL_MODE_FRONT);
					p->flushAndBind(cmd_list);

					EntityRenderer::ShadowUBO ubo;
					ubo.light_space_matrix = light_matrix;
					ubo.light_pos = glm::vec4(position, 1.0);
					ubo.z_far = 0;

					gDynamicRHI->setConstantBufferData(1, &ubo, sizeof(EntityRenderer::ShadowUBO));

					BoundFrustum bound_frustum(light_matrix, glm::mat4(1.0f));
					for (const RenderBatch &batch : batches)
					{
						if (!batch.world_bound_box.isInside(bound_frustum))
							continue;

						struct ShadowPushConstact
						{
							uint32_t instance_id;
						} push_constant;

						push_constant.instance_id = batch.instance_id;
						gDynamicRHI->setConstantBufferData(2, &push_constant, sizeof(ShadowPushConstact));

						cmd_list->setIndexBuffer(batch.mesh->indexBuffer);
						cmd_list->drawIndexedInstanced(batch.mesh->indices.size(), 1, 0, 0, 0);
						Renderer::addDrawCalls(1);
					}

					cmd_list->resetRenderTargets();
				}
			});
		}
	}
}

void ShadowRenderer::addRayTracedShadowPasses(FrameGraph & fg, Ref<RayTracingScene> rt_scene)
{
	if (!rt_scene || !rt_scene->getTopLevelAS())
		return;

	RayTracedShadowPass &shadow_pass = fg.getBlackboard().add<RayTracedShadowPass>();

	shadow_pass = fg.addCallbackPass<RayTracedShadowPass>("Ray Traced Shadows Pass",
	[&](RenderPassBuilder &builder, RayTracedShadowPass &data)
	{
		data.visibility = builder.createTexture("Ray Traced Lighting Image", Renderer::getViewportWidth(), Renderer::getViewportHeight(), gDynamicRHI->getSwapchainTexture(0)->getDescription().format);
		data.visibility = builder.writeUAVTexture(data.visibility, TEXTURE_RESOURCE_ACCESS_GENERAL); // was transfer

		builder.readTexture(GFXRID(GBufferDepth));
	},
	[=](const RayTracedShadowPass &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto visiblity = resources.getTexture(data.visibility);

		gGlobalPipeline->setupRayTracing(raygen_shader, miss_shader, closest_hit_shader);
		gGlobalPipeline->flushAndBind(cmd_list);

		struct LightUBO
		{
			glm::vec4 dir_light_direction;
			uint32_t depth_texture_id;
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

		ubo_light.depth_texture_id = resources.getBindlessId(GFXRID(GBufferDepth));

		auto ray_traced_lighting = visiblity;

		gDynamicRHI->setConstantBufferData(1, &ubo_light, sizeof(LightUBO));

		gDynamicRHI->setUAVTexture(0, ray_traced_lighting);
		gDynamicRHI->setAccelerationStructure(2, rt_scene->getTopLevelAS());

		cmd_list->dispatchRays(Renderer::getViewportSize().x, Renderer::getViewportSize().y, 1);

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
		}
	}
}

void ShadowRenderer::update_cascades(LightComponent &light, glm::vec3 light_dir, Camera *camera)
{
	float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

	float nearClip = camera->getNear();
	float farClip = camera->getFar();
	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	// Calculate split depths based on view camera frustum
	// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
	{
		float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = 0.95 * (log - uniform) + uniform;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}
	
	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
	{
		float splitDist = cascadeSplits[i];

		glm::vec3 frustumCorners[8] = {
			glm::vec3(-1.0f, 1.0f, 0.0f),
			glm::vec3(1.0f, 1.0f, 0.0f),
			glm::vec3(1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f, -1.0f, 0.0f),

			glm::vec3(-1.0f, 1.0f,  1.0f),
			glm::vec3(1.0f, 1.0f,  1.0f),
			glm::vec3(1.0f, -1.0f,  1.0f),
			glm::vec3(-1.0f, -1.0f,  1.0f),
		};

		// Project frustum corners into world space
		glm::mat4 invCam = glm::inverse(camera->getProj() * camera->getView());
		for (uint32_t j = 0; j < 8; j++)
		{
			glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
			frustumCorners[j] = invCorner / invCorner.w;
		}

		for (uint32_t j = 0; j < 4; j++)
		{
			glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
			frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
			frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
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
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

		glm::mat4 lightViewMatrix = glm::lookAtRH(frustumCenter - light_dir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightOrthoMatrix = glm::orthoRH(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f - 150.0f, maxExtents.z - minExtents.z + 150.0f);

		// Fix shimmering
		float shadow_map_size = 4096;
		glm::vec2 shadow_origin = (lightOrthoMatrix * lightViewMatrix * glm::vec4(0, 0, 0, 1)) * shadow_map_size / 2.0f;
		glm::vec2 round_offset = glm::round(shadow_origin) - shadow_origin;
		round_offset = round_offset * 2.0f / shadow_map_size;
		lightOrthoMatrix[3] += glm::vec4(round_offset, 0, 0);

		// Store split distance and matrix in cascade
		light.cascades[i].splitDepth = (nearClip + splitDist * clipRange);
		light.cascades[i].viewProjMatrix = lightOrthoMatrix * lightViewMatrix;

		lastSplitDist = cascadeSplits[i];
	}
}
