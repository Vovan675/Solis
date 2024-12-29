#include "pch.h"
#include "ShadowRenderer.h"
#include "RHI/VkWrapper.h"
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
	shadows_vertex_shader = Shader::create("shaders/lighting/shadows.vert", Shader::VERTEX_SHADER);
	shadows_fragment_shader_point = Shader::create("shaders/lighting/shadows.frag", Shader::FRAGMENT_SHADER, {{"LIGHT_TYPE", "0"}});
	shadows_fragment_shader_directional = Shader::create("shaders/lighting/shadows.frag", Shader::FRAGMENT_SHADER, {{"LIGHT_TYPE", "1"}});

	if (engine_ray_tracing)
	{
		raygen_shader = Shader::create("shaders/rt/raygen.rgen", Shader::RAY_GENERATION_SHADER);
		miss_shader = Shader::create("shaders/rt/miss.rmiss", Shader::MISS_SHADER);

		closest_hit_shader = Shader::create("shaders/rt/closesthit.rchit", Shader::CLOSEST_HIT_SHADER);
		TextureDescription tex_description = {};
		tex_description.width = VkWrapper::swapchain->swap_extent.width;
		tex_description.height = VkWrapper::swapchain->swap_extent.height;
		tex_description.image_format = VK_FORMAT_B8G8R8A8_UNORM;
		tex_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC | TEXTURE_USAGE_STORAGE;
		storage_image = Texture::create(tex_description);
		storage_image->fill();
	}
}

void ShadowRenderer::addShadowMapPasses(FrameGraph &fg)
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
			faces_transforms.push_back(glm::lookAt(position, position + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)));
			faces_transforms.push_back(glm::lookAt(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)));
			faces_transforms.push_back(glm::lookAt(position, position + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)));
			faces_transforms.push_back(glm::lookAt(position, position + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)));
			faces_transforms.push_back(glm::lookAt(position, position + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)));
			faces_transforms.push_back(glm::lookAt(position, position + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0)));


			FrameGraphResource shadow_map_resource = importTexture(fg, light.shadow_map);

			fg.addCallbackPass<ShadowPasses>("Cube Shadow Map Pass",
			[&](RenderPassBuilder &builder, ShadowPasses &data)
			{
				shadow_passes.shadow_maps.push_back(builder.write(shadow_map_resource));
			},
			[=, &shadow_passes](const ShadowPasses &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
			{
				// Render
				auto &shadow_map = resources.getResource<FrameGraphTexture>(shadow_passes.shadow_maps.back());

				for (int face = 0; face < 6; face++)
				{
					if (faces_transforms.size() <= face)
						continue;

					glm::mat4 light_projection = glm::perspectiveRH(glm::radians(90.0f), 1.0f, 0.1f, light.radius);
					glm::mat4 light_matrix = light_projection * faces_transforms[face];

					VkWrapper::cmdBeginRendering(command_buffer, {}, shadow_map.texture, face);

					auto &p = VkWrapper::global_pipeline;
					p->reset();

					p->setVertexShader(shadows_vertex_shader);
					p->setFragmentShader(shadows_fragment_shader_point);

					p->setRenderTargets(VkWrapper::current_render_targets);
					p->setUseBlending(false);
					p->setCullMode(VK_CULL_MODE_FRONT_BIT);

					p->flush();
					p->bind(command_buffer);

					EntityRenderer::ShadowUBO ubo;
					ubo.light_space_matrix = light_matrix;
					ubo.light_pos = glm::vec4(position, 1.0);
					ubo.z_far = light.radius;

					Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(EntityRenderer::ShadowUBO));
					Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

					auto mesh_entities_view = Scene::getCurrentScene()->getEntitiesWith<TransformComponent, MeshRendererComponent>();
					for (auto mesh_entity_id : mesh_entities_view)
					{
						Entity entity(mesh_entity_id);
						auto [transform, mesh] = mesh_entities_view.get<TransformComponent, MeshRendererComponent>(mesh_entity_id);
						entity_renderer->renderEntityShadow(command_buffer, entity.getWorldTransformMatrix(), mesh);
					}
					p->unbind(command_buffer);

					VkWrapper::cmdEndRendering(command_buffer);
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
			[=](const CascadeShadowPass &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
			{
				// Render
				auto &shadow_map = resources.getResource<FrameGraphTexture>(data.shadow_map);

				for (int c = 0; c < SHADOW_MAP_CASCADE_COUNT; c++)
				{
					glm::mat4 light_matrix = light.cascades[c].viewProjMatrix;

					VkWrapper::cmdBeginRendering(command_buffer, {}, shadow_map.texture, c);

					auto &p = VkWrapper::global_pipeline;
					p->reset();

					p->setVertexShader(shadows_vertex_shader);
					p->setFragmentShader(shadows_fragment_shader_directional);

					p->setRenderTargets(VkWrapper::current_render_targets);
					p->setUseBlending(false);
					p->setCullMode(VK_CULL_MODE_FRONT_BIT);

					p->flush();
					p->bind(command_buffer);

					EntityRenderer::ShadowUBO ubo;
					ubo.light_space_matrix = light_matrix;
					ubo.light_pos = glm::vec4(position, 1.0);
					ubo.z_far = 0;

					Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(EntityRenderer::ShadowUBO));
					Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

					auto mesh_entities_view = Scene::getCurrentScene()->getEntitiesWith<TransformComponent, MeshRendererComponent>();
					for (auto mesh_entity_id : mesh_entities_view)
					{
						Entity entity(mesh_entity_id);
						auto [transform, mesh] = mesh_entities_view.get<TransformComponent, MeshRendererComponent>(mesh_entity_id);
						entity_renderer->renderEntityShadow(command_buffer, entity.getWorldTransformMatrix(), mesh);
					}
					p->unbind(command_buffer);
					VkWrapper::cmdEndRendering(command_buffer);
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
		desc.image_format = VkWrapper::swapchain->surface_format.format;
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT | TEXTURE_USAGE_STORAGE;
		desc.debug_name = "Ray Traced Lighting Image";

		data.visibility = builder.createResource<FrameGraphTexture>(desc.debug_name, desc);
		data.visibility = builder.write(data.visibility, TEXTURE_RESOURCE_ACCESS_GENERAL); // was transfer
	},
	[=](const RayTracedShadowPass &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
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
		raygenShaderSbtEntry.deviceAddress = VkWrapper::getBufferDeviceAddress(p->current_pipeline->raygenShaderBindingTable->bufferHandle);
		raygenShaderSbtEntry.stride = handleSizeAligned;
		raygenShaderSbtEntry.size = handleSizeAligned;

		VkStridedDeviceAddressRegionKHR missShaderSbtEntry{};
		missShaderSbtEntry.deviceAddress = VkWrapper::getBufferDeviceAddress(p->current_pipeline->missShaderBindingTable->bufferHandle);
		missShaderSbtEntry.stride = handleSizeAligned;
		missShaderSbtEntry.size = handleSizeAligned;

		VkStridedDeviceAddressRegionKHR hitShaderSbtEntry{};
		hitShaderSbtEntry.deviceAddress = VkWrapper::getBufferDeviceAddress(p->current_pipeline->hitShaderBindingTable->bufferHandle);
		hitShaderSbtEntry.stride = handleSizeAligned;
		hitShaderSbtEntry.size = handleSizeAligned;

		VkStridedDeviceAddressRegionKHR callableShaderSbtEntry{};

		//ray_traced_lighting->transitLayout(command_buffer, TEXTURE_LAYOUT_GENERAL);

		VkUtils::vkCmdTraceRaysKHR(command_buffer.get_buffer(), &raygenShaderSbtEntry, &missShaderSbtEntry, &hitShaderSbtEntry, &callableShaderSbtEntry,
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

	float nearClip = context->editor_camera->getNear();
	float farClip = context->editor_camera->getFar();
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
	cascadeSplits[3] = 0.3f;
	/*
	cascadeSplits[0] = 0.05f;
	cascadeSplits[1] = 0.15f;
	cascadeSplits[2] = 0.3f;
	cascadeSplits[3] = 1.0f;
	*/
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
		glm::mat4 invCam = glm::inverse(context->editor_camera->getProj() * context->editor_camera->getView());
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

		glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - light_dir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f - 50.0f, maxExtents.z - minExtents.z + 50.0f);

		// Store split distance and matrix in cascade
		light.cascades[i].splitDepth = (context->editor_camera->getNear() + splitDist * clipRange) * -1.0f;
		light.cascades[i].viewProjMatrix = lightOrthoMatrix * lightViewMatrix;

		lastSplitDist = cascadeSplits[i];
	}
}
