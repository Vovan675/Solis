#include "pch.h"
#include "ShadowRenderer.h"
#include "Rendering/Renderer.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "FrameGraph/FrameGraph.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphUtils.h"
#include "EntityRenderer.h"
#include "glm/glm.hpp"
#include "Editor/EditorContext.h"
#include "Camera.h"
#include "Variables.h"

ShadowRenderer::ShadowRenderer()
{
	shadows_vertex_shader = gDynamicRHI->createShader(L"shaders/lighting/shadows.hlsl", VERTEX_SHADER);
	shadows_fragment_shader_point = gDynamicRHI->createShader(L"shaders/lighting/shadows.hlsl", FRAGMENT_SHADER, {{"LIGHT_TYPE", "0"}});
	shadows_fragment_shader_directional = gDynamicRHI->createShader(L"shaders/lighting/shadows.hlsl", FRAGMENT_SHADER, {{"LIGHT_TYPE", "1"}});

	if (engine_ray_tracing)
	{
		raygen_shader = gDynamicRHI->createShader(L"shaders/rt/raygen.rgen", RAY_GENERATION_SHADER);
		miss_shader = gDynamicRHI->createShader(L"shaders/rt/miss.rmiss", MISS_SHADER);

		closest_hit_shader = gDynamicRHI->createShader(L"shaders/rt/closesthit.rchit", CLOSEST_HIT_SHADER);
		TextureDescription tex_description = {};
		tex_description.width = gDynamicRHI->getSwapchainTexture(0)->getWidth();
		tex_description.height = gDynamicRHI->getSwapchainTexture(0)->getHeight();
		tex_description.format = FORMAT_R8G8B8A8_UNORM;
		tex_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC | TEXTURE_USAGE_STORAGE;
		storage_image = gDynamicRHI->createTexture(tex_description);
		storage_image->fill();
	}
}

void ShadowRenderer::addShadowMapPasses(FrameGraph &fg, const std::vector<RenderBatch> &batches)
{
	auto light_entities_id = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();

	ShadowPasses &shadow_passes = fg.getBlackboard().add<ShadowPasses>();
	CascadeShadowPass &cs_pass = fg.getBlackboard().add<CascadeShadowPass>();

	for (entt::entity light_entity_id : light_entities_id)
	{
		Entity light_entity(light_entity_id);
		auto &light = light_entity.getComponent<LightComponent>();

		//light.shadow_map->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);

		glm::vec3 scale, position, skew;
		glm::vec4 persp;
		glm::quat rotation;
		glm::decompose(light_entity.getWorldTransformMatrix(), scale, rotation, position, skew, persp);


		if (light.getType() == LIGHT_TYPE_POINT)
		{
			std::vector<glm::mat4> faces_transforms;
			faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)));
			faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0)));
			faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)));
			faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, -1, 0), glm::vec3(0, 0, 1)));
			faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, 0, 1), glm::vec3(0, 1, 0)));
			faces_transforms.push_back(glm::lookAtLH(position, position + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)));


			FrameGraphResource shadow_map_resource = importTexture(fg, light.shadow_map);

			fg.addCallbackPass<ShadowPasses>("Cube Shadow Map Pass",
			[&](RenderPassBuilder &builder, ShadowPasses &data)
			{
				shadow_passes.shadow_maps.push_back(builder.write(shadow_map_resource));
			},
			[=, &shadow_passes, &batches](const ShadowPasses &data, const RenderPassResources &resources, RHICommandList *cmd_list)
			{
				// Render
				auto &shadow_map = resources.getResource<FrameGraphTexture>(shadow_passes.shadow_maps.back());

				for (int face = 0; face < 6; face++)
				{
					if (faces_transforms.size() <= face)
						continue;

					glm::mat4 light_projection = glm::perspectiveLH(glm::radians(90.0f), 1.0f, 0.05f, light.radius);
					glm::mat4 light_matrix = light_projection * faces_transforms[face];

					cmd_list->setRenderTargets({}, shadow_map.texture, face, 0, true);

					auto &p = gGlobalPipeline;
					p->reset();

					p->setVertexShader(shadows_vertex_shader);
					p->setFragmentShader(shadows_fragment_shader_point);

					p->setVertexInputsDescription(Engine::Vertex::GetVertexInputsDescription());
					p->setRenderTargets(cmd_list->getCurrentRenderTargets());
					p->setUseBlending(false);
					p->setCullMode(CULL_MODE_FRONT);

					p->flush();
					p->bind(cmd_list);

					EntityRenderer::ShadowUBO ubo;
					ubo.light_space_matrix = light_matrix;
					ubo.light_pos = glm::vec4(position, 1.0);
					ubo.z_far = light.radius;

					gDynamicRHI->setConstantBufferData(1, &ubo, sizeof(EntityRenderer::ShadowUBO));

					BoundFrustum bound_frustum(light_projection, faces_transforms[face]);
					for (const RenderBatch &batch : batches)
					{
						if (!(batch.mesh->bound_box * batch.world_transform).isInside(bound_frustum))
							continue;

						struct ShadowPushConstact
						{
							glm::mat4 model;
						} push_constant;

						push_constant.model = batch.world_transform;
						gDynamicRHI->setConstantBufferData(0, &push_constant, sizeof(ShadowPushConstact));

						// Render mesh
						cmd_list->setVertexBuffer(batch.mesh->vertexBuffer);
						cmd_list->setIndexBuffer(batch.mesh->indexBuffer);
						cmd_list->drawIndexedInstanced(batch.mesh->indices.size(), 1, 0, 0, 0);
						Renderer::addDrawCalls(1);
					}
					
					p->unbind(cmd_list);
					cmd_list->resetRenderTargets();
				}
			});
		} else
		{
			FrameGraphResource shadow_map_resource = importTexture(fg, light.shadow_map);

			glm::vec3 light_dir = light_entity.getLocalDirection(glm::vec3(0, 0, 1));
			debug_renderer->addArrow(position, position + light_dir, 0.1);
			update_cascades(light, light_dir);

			cs_pass = fg.addCallbackPass<CascadeShadowPass>("Cascaded Shadows Pass",
			[&](RenderPassBuilder &builder, CascadeShadowPass &data)
			{
				data.shadow_map = builder.write(shadow_map_resource);
			},
			[=, &batches](const CascadeShadowPass &data, const RenderPassResources &resources, RHICommandList *cmd_list)
			{
				// Render
				auto &shadow_map = resources.getResource<FrameGraphTexture>(data.shadow_map);

				for (int c = 0; c < SHADOW_MAP_CASCADE_COUNT; c++)
				{
					glm::mat4 light_matrix = light.cascades[c].viewProjMatrix;

					cmd_list->setRenderTargets({}, shadow_map.texture, c, 0, true);

					auto &p = gGlobalPipeline;
					p->reset();

					p->setVertexShader(shadows_vertex_shader);
					p->setFragmentShader(shadows_fragment_shader_directional);

					p->setVertexInputsDescription(Engine::Vertex::GetVertexInputsDescription());
					p->setRenderTargets(cmd_list->getCurrentRenderTargets());
					p->setUseBlending(false);
					p->setCullMode(CULL_MODE_FRONT);

					p->flush();
					p->bind(cmd_list);

					EntityRenderer::ShadowUBO ubo;
					ubo.light_space_matrix = light_matrix;
					ubo.light_pos = glm::vec4(position, 1.0);
					ubo.z_far = 0;

					gDynamicRHI->setConstantBufferData(1, &ubo, sizeof(EntityRenderer::ShadowUBO));

					BoundFrustum bound_frustum(light_matrix, glm::mat4(1.0f));
					for (const RenderBatch &batch : batches)
					{
						if (!(batch.mesh->bound_box * batch.world_transform).isInside(bound_frustum))
							continue;

						struct ShadowPushConstact
						{
							alignas(16) glm::mat4 model;
						} push_constant;

						push_constant.model = batch.world_transform;
						gDynamicRHI->setConstantBufferData(0, &push_constant, sizeof(ShadowPushConstact));


						cmd_list->setVertexBuffer(batch.mesh->vertexBuffer);
						cmd_list->setIndexBuffer(batch.mesh->indexBuffer);
						cmd_list->drawIndexedInstanced(batch.mesh->indices.size(), 1, 0, 0, 0);
						Renderer::addDrawCalls(1);
					}

					p->unbind(cmd_list);
					cmd_list->resetRenderTargets();
				}
			});
		}
	}
}

void ShadowRenderer::addRayTracedShadowPasses(FrameGraph & fg)
{
	if (!ray_tracing_scene || !ray_tracing_scene->getTopLevelAS().handle)
		return;

	RayTracedShadowPass &shadow_pass = fg.getBlackboard().add<RayTracedShadowPass>();

	shadow_pass = fg.addCallbackPass<RayTracedShadowPass>("Ray Traced Shadows Pass",
	[&](RenderPassBuilder &builder, RayTracedShadowPass &data)
	{
		FrameGraphTexture::Description desc;
		desc.width = Renderer::getViewportSize().x;
		desc.height = Renderer::getViewportSize().y;
		desc.format = gDynamicRHI->getSwapchainTexture(0)->getDescription().format;
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT | TEXTURE_USAGE_STORAGE;
		desc.debug_name = "Ray Traced Lighting Image";

		data.visibility = builder.createResource<FrameGraphTexture>(desc.debug_name, desc);
		data.visibility = builder.write(data.visibility, TEXTURE_RESOURCE_ACCESS_GENERAL); // was transfer
	},
	[=](const RayTracedShadowPass &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		/*
		auto &visiblity = resources.getResource<FrameGraphTexture>(data.visibility);

		auto p = VkWrapper::global_pipeline;
		p->reset();

		p->setIsRayTracingPipeline(true);
		p->setRayGenerationShader(raygen_shader);
		p->setMissShader(miss_shader);
		p->setClosestHitShader(closest_hit_shader);

		p->flush();
		p->bind(command_buffer);

		// Uniforms
		struct UBO 
		{
			glm::mat4 viewInverse;
			glm::mat4 projInverse;
		} ubo;

		struct LightUBO
		{
			glm::vec4 dir_light_direction;
		} ubo_light;

		ubo.viewInverse = glm::inverse(context->editor_camera->getView());
		ubo.projInverse = glm::inverse(context->editor_camera->getProj());

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

		auto &ray_traced_lighting = visiblity.texture;
		Renderer::setShadersAccelerationStructure(p->getCurrentShaders(), &ray_tracing_scene->getTopLevelAS().handle, 0);
		Renderer::setShadersTexture(p->getCurrentShaders(), 1, ray_traced_lighting, -1, -1, true);
		Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 2, &ubo, sizeof(UBO));
		Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 6, &ubo_light, sizeof(LightUBO));

		Renderer::setShadersStorageBuffer(p->getCurrentShaders(), 3, ray_tracing_scene->getObjDescs().data(), sizeof(RayTracingScene::ObjDesc) * ray_tracing_scene->getObjDescs().size());

		Renderer::setShadersStorageBuffer(p->getCurrentShaders(), 4, ray_tracing_scene->getBigVertexBuffer());
		Renderer::setShadersStorageBuffer(p->getCurrentShaders(), 5, ray_tracing_scene->getBigIndexBuffer());
		Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);

		const uint32_t handleSizeAligned = VkWrapper::alignedSize(VkWrapper::device->physicalRayTracingProperties.shaderGroupHandleSize, VkWrapper::device->physicalRayTracingProperties.shaderGroupHandleAlignment);
		VkStridedDeviceAddressRegionKHR raygenShaderSbtEntry{};
		// TODO: fix
		//raygenShaderSbtEntry.deviceAddress = VkWrapper::getBufferDeviceAddress(p->current_pipeline->raygenShaderBindingTable->buffer_handle);
		raygenShaderSbtEntry.stride = handleSizeAligned;
		raygenShaderSbtEntry.size = handleSizeAligned;

		VkStridedDeviceAddressRegionKHR missShaderSbtEntry{};
		// TODO: fix
		//missShaderSbtEntry.deviceAddress = VkWrapper::getBufferDeviceAddress(p->current_pipeline->missShaderBindingTable->buffer_handle);
		missShaderSbtEntry.stride = handleSizeAligned;
		missShaderSbtEntry.size = handleSizeAligned;

		VkStridedDeviceAddressRegionKHR hitShaderSbtEntry{};
		// TODO: fix
		//hitShaderSbtEntry.deviceAddress = VkWrapper::getBufferDeviceAddress(p->current_pipeline->hitShaderBindingTable->buffer_handle);
		hitShaderSbtEntry.stride = handleSizeAligned;
		hitShaderSbtEntry.size = handleSizeAligned;

		VkStridedDeviceAddressRegionKHR callableShaderSbtEntry{};

		//ray_traced_lighting->transitLayout(command_buffer, TEXTURE_LAYOUT_GENERAL);

		VulkanUtils::vkCmdTraceRaysKHR(command_buffer.get_buffer(), &raygenShaderSbtEntry, &missShaderSbtEntry, &hitShaderSbtEntry, &callableShaderSbtEntry,
						  Renderer::getViewportSize().x, Renderer::getViewportSize().y, 1);
		p->unbind(command_buffer);

		//ray_traced_lighting->transitLayout(command_buffer, TEXTURE_LAYOUT_TRANSFER_SRC);
		/*
		Renderer::getRenderTarget(RENDER_TARGET_RAY_TRACED_LIGHTING)->transitLayout(command_buffer, TEXTURE_LAYOUT_TRANSFER_DST);

		VkImageCopy copyRegion{};
		copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copyRegion.srcOffset = {0, 0, 0};
		copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copyRegion.dstOffset = {0, 0, 0};
		copyRegion.extent = {VkWrapper::swapchain->swap_extent.width, VkWrapper::swapchain->swap_extent.height, 1};
		vkCmdCopyImage(command_buffer.get_buffer(), storage_image->getRawResource().lock()->imageHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Renderer::getRenderTarget(RENDER_TARGET_RAY_TRACED_LIGHTING)->getRawResource().lock()->imageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		Renderer::getRenderTarget(RENDER_TARGET_RAY_TRACED_LIGHTING)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
		*/
	});
}

void ShadowRenderer::update_cascades(LightComponent &light, glm::vec3 light_dir)
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
	// TODO: calculate
	//cascadeSplits[3] = 0.3f;
	//cascadeSplits[0] = 0.05f;
	//cascadeSplits[1] = 0.15f;
	//cascadeSplits[2] = 0.3f;
	//cascadeSplits[3] = 1.0f;
	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
	{
		float splitDist = cascadeSplits[i];

		glm::vec3 frustumCorners[8] = {
			glm::vec3(-1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f,  1.0f, 0.0f),
			glm::vec3(1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f, -1.0f, 0.0f),
			glm::vec3(-1.0f,  1.0f,  1.0f),
			glm::vec3(1.0f,  1.0f,  1.0f),
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

		glm::mat4 lightViewMatrix = glm::lookAtLH(frustumCenter - light_dir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightOrthoMatrix = glm::orthoLH(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f - 50.0f, maxExtents.z - minExtents.z + 50.0f);

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
