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
#include "Rendering/GlobalBufferCache.h"

ShadowRenderer::ShadowRenderer()
{
	shadows_vertex_shader = gDynamicRHI->createShader(L"shaders/lighting/shadows.hlsl", VERTEX_SHADER);
	shadows_fragment_shader_point = gDynamicRHI->createShader(L"shaders/lighting/shadows.hlsl", FRAGMENT_SHADER, "PSMain", {{"LIGHT_TYPE", "0"}});
	shadows_fragment_shader_directional = gDynamicRHI->createShader(L"shaders/lighting/shadows.hlsl", FRAGMENT_SHADER, "PSMain", {{"LIGHT_TYPE", "1"}});

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

void ShadowRenderer::addShadowMapPasses(FrameGraph &fg, uint32_t max_draw_calls_count, RHIBufferRef instances_pass_masks_gpu)
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

			for (int face_ = 0; face_ < 6; face_++)
			{
				int face = face_;

				glm::mat4 light_projection = glm::perspectiveLH(glm::radians(90.0f), 1.0f, 0.05f, light.radius);
				glm::mat4 light_view_projection = light_projection * faces_transforms[face];
				DrawCallsArguments draw_calls_args = create_draw_calls(fg, max_draw_calls_count, instances_pass_masks_gpu, PASS_MASK_POINT_SHADOW, light_view_projection);

				fg.addCallbackPass<ShadowPassData>("Cube Shadow Map Pass",
				[&](RenderPassBuilder &builder, ShadowPassData &data)
				{
					data.shadow_map = builder.writeTexture(shadow_map_resource);
					if (face == 0)
						shadow_passes.shadow_maps.push_back(data.shadow_map);
				},
				[=](const ShadowPassData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
				{
					// Render
					auto shadow_map = resources.getTexture(data.shadow_map);

					VertexInputsDescription inputs_desc;
					inputs_desc.inputs.push_back({"INSTANCE_ID", 1, FORMAT_R32_UINT, true});

					cmd_list->setVertexBuffer(GlobalBufferCache::getGlobalVertexBuffer(), 0, sizeof(Engine::Vertex), 0);
					cmd_list->setVertexBuffer(draw_calls_args.draw_calls_instances_gpu, 0, sizeof(uint32_t), 1);
					cmd_list->setIndexBuffer(GlobalBufferCache::getGlobalIndexBuffer(), 0, IndexFormat::UINT32);

					draw_calls_args.draw_indexed_args_gpu->transitState(ResourceState::INDIRECT_ARGS);
					draw_calls_args.draw_indexed_count_gpu->transitState(ResourceState::INDIRECT_ARGS);

					cmd_list->setRenderTargets({}, shadow_map, face, 0, true, 1.0f);
					gGlobalPipeline->setupGraphicsPipeline(cmd_list, shadows_vertex_shader, shadows_fragment_shader_point, inputs_desc, false, true, CULL_MODE_FRONT);
					gGlobalPipeline->setDepthFunc(COMPARE_FUNC_LESS_EQUAL);
					gGlobalPipeline->flushAndBind(cmd_list);

					EntityRenderer::ShadowUBO ubo;
					ubo.light_space_matrix = light_view_projection;
					ubo.light_pos = glm::vec4(position, 1.0);
					ubo.z_far = light.radius;

					gDynamicRHI->setConstantBufferData(1, &ubo, sizeof(EntityRenderer::ShadowUBO));

					cmd_list->drawIndexedIndirect(draw_calls_args.draw_indexed_args_gpu, max_draw_calls_count, draw_calls_args.draw_indexed_count_gpu);

					cmd_list->resetRenderTargets();
				});
			}
		} else
		{
			FrameGraphTextureId shadow_map_resource = fg.importTexture(GraphicsResourceName((eastl::string("Shadow Map ") + eastl::to_string((uint32_t)light_entity_id)).c_str()), light.shadow_map);

			for (int i_ = 0; i_ < SHADOW_MAP_CASCADE_COUNT; i_++)
			{
				int cascade = i_;
				DrawCallsArguments draw_calls_args = create_draw_calls(fg, max_draw_calls_count, instances_pass_masks_gpu, PASS_MASK_DIRECTIONAL_SHADOW, light.cascades[cascade].viewProjMatrix);

				fg.addCallbackPass<ShadowPassData>("Cascaded Shadows Pass",
				[&](RenderPassBuilder &builder, ShadowPassData &data)
				{
					data.shadow_map = builder.writeTexture(shadow_map_resource);
					if (cascade == 0)
						shadow_passes.shadow_maps.push_back(data.shadow_map);
				},
				[=, &light](const ShadowPassData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
				{
					// Render
					auto shadow_map = resources.getTexture(data.shadow_map);
					glm::mat4 light_matrix = light.cascades[cascade].viewProjMatrix;

					VertexInputsDescription inputs_desc;
					inputs_desc.inputs.push_back({"INSTANCE_ID", 1, FORMAT_R32_UINT, true});

					cmd_list->setVertexBuffer(GlobalBufferCache::getGlobalVertexBuffer(), 0, sizeof(Engine::Vertex), 0);
					cmd_list->setVertexBuffer(draw_calls_args.draw_calls_instances_gpu, 0, sizeof(uint32_t), 1);
					cmd_list->setIndexBuffer(GlobalBufferCache::getGlobalIndexBuffer(), 0, IndexFormat::UINT32);

					draw_calls_args.draw_indexed_args_gpu->transitState(ResourceState::INDIRECT_ARGS);
					draw_calls_args.draw_indexed_count_gpu->transitState(ResourceState::INDIRECT_ARGS);

					cmd_list->setRenderTargets({}, shadow_map, cascade, 0, true, 1.0f);
					gGlobalPipeline->setupGraphicsPipeline(cmd_list, shadows_vertex_shader, shadows_fragment_shader_directional, inputs_desc, false, true, CULL_MODE_FRONT);
					gGlobalPipeline->setDepthFunc(COMPARE_FUNC_LESS_EQUAL);
					gGlobalPipeline->flushAndBind(cmd_list);

					EntityRenderer::ShadowUBO ubo;
					ubo.light_space_matrix = light_matrix;
					ubo.light_pos = glm::vec4(position, 1.0);
					ubo.z_far = 0;

					gDynamicRHI->setConstantBufferData(1, &ubo, sizeof(EntityRenderer::ShadowUBO));

					cmd_list->drawIndexedIndirect(draw_calls_args.draw_indexed_args_gpu, max_draw_calls_count, draw_calls_args.draw_indexed_count_gpu);

					cmd_list->resetRenderTargets();
				});
				}
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

		ubo_light.depth_texture_id = resources.getBindlessId(GFXRID(GBufferDepth));

		auto ray_traced_lighting = visiblity;
		ubo_light.output_texture_id = ray_traced_lighting->getUnorderedAccessView()->getBindlessIndex();

		gDynamicRHI->setConstantBufferData(1, &ubo_light, sizeof(LightUBO));

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

ShadowRenderer::DrawCallsArguments &ShadowRenderer::create_draw_calls(FrameGraph &fg, uint32_t max_draw_calls_count, RHIBufferRef instances_pass_masks_gpu, uint32_t pass_mask, glm::float4x4 view_projection)
{
	auto fill_buffer = [](RHIBufferRef &buffer, void *data, size_t count, size_t stride, const char *name, BufferUsage usage = BufferUsage::SHADER_READ_BUFFER, bool use_staging = false)
	{
		uint32_t buffer_size = count * stride;
		if (!buffer || buffer->getSize() < buffer_size)
		{
			BufferDescription desc;
			desc.size = buffer_size;
			desc.usage = usage;
			desc.use_staging_buffer = use_staging;
			desc.storage_stride = stride;
			buffer = gDynamicRHI->createBuffer(desc);
			buffer->setDebugName(name);
		}

		if (data)
			buffer->fill(data);
	};

	fill_buffer(arguments.draw_indexed_args_gpu, nullptr, max_draw_calls_count, sizeof(DrawIndexedIndirect), "Draw Indexed Args", BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER, true);
	fill_buffer(arguments.draw_indexed_count_gpu, nullptr, 1, sizeof(uint32_t), "Draw Indexed Count", BufferUsage::INDIRECT_ARGS_BUFFER | BufferUsage::SHADER_WRITE_BUFFER, true);
	fill_buffer(arguments.draw_calls_instances_gpu, nullptr, max_draw_calls_count, sizeof(uint32_t), "Draw Calls Instances", BufferUsage::VERTEX_BUFFER | BufferUsage::SHADER_WRITE_BUFFER, true);

	fg.addCallbackPass<EmptyData>("Create Draw Calls Pass (Shadows)",
	[&](RenderPassBuilder &builder, EmptyData &data)
	{
		builder.setSideEffect(true);
	},
	[=](const EmptyData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		arguments.draw_indexed_args_gpu->transitState(ResourceState::UAV);
		arguments.draw_indexed_count_gpu->transitState(ResourceState::UAV);
		arguments.draw_calls_instances_gpu->transitState(ResourceState::UAV);
		instances_pass_masks_gpu->transitState(ResourceState::UAV);

		{
			struct Constants
			{
				uint32_t draw_calls_count_buffer_id;
			} constants;
			constants.draw_calls_count_buffer_id = arguments.draw_indexed_count_gpu->getUnorderedAccessView()->getBindlessIndex();

			gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/init_draw_calls.hlsl", COMPUTE_SHADER));
			gGlobalPipeline->flushAndBind(cmd_list);

			gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
			cmd_list->dispatch(1, 1, 1);
		}

		arguments.draw_indexed_args_gpu->transitState(ResourceState::UAV);
		arguments.draw_indexed_count_gpu->transitState(ResourceState::UAV);
		arguments.draw_calls_instances_gpu->transitState(ResourceState::UAV);

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/create_draw_calls_shadows.hlsl", COMPUTE_SHADER, "CSMain", {{"IS_ORTHO_FRUSTUM", instances_pass_masks_gpu == PASS_MASK_DIRECTIONAL_SHADOW ? "1" : "0"}}));


		gGlobalPipeline->flushAndBind(cmd_list);

		struct Constants
		{
			glm::mat4 frustum_view_projection;
			uint32_t draw_indexed_args_buffer_id;
			uint32_t draw_indexed_count_buffer_id;
			uint32_t draw_calls_indirect_instances_buffer_id;
			uint32_t instances_pass_mask_buffer_id;
			uint32_t instances_count;
			uint32_t current_pass_mask;
			uint32_t pad[2];
		} constants;

		constants.frustum_view_projection = view_projection;
		constants.draw_indexed_args_buffer_id = arguments.draw_indexed_args_gpu->getUnorderedAccessView()->getBindlessIndex();
		constants.draw_indexed_count_buffer_id = arguments.draw_indexed_count_gpu->getUnorderedAccessView()->getBindlessIndex();
		constants.draw_calls_indirect_instances_buffer_id = arguments.draw_calls_instances_gpu->getUnorderedAccessView()->getBindlessIndex();
		constants.instances_pass_mask_buffer_id = instances_pass_masks_gpu->getUnorderedAccessView()->getBindlessIndex();
		constants.instances_count = max_draw_calls_count;
		constants.current_pass_mask = pass_mask;

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		int num_groups = ceil(constants.instances_count / 32.0f);
		cmd_list->dispatch(num_groups, 1, 1);
		instances_pass_masks_gpu->transitState(ResourceState::UAV);
	});

	return arguments;
}
