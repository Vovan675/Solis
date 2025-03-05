#include "pch.h"
#include "DefferedLightingRenderer.h"
#include "imgui.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Scene/Entity.h"
#include "Model.h"
#include "Variables.h"

DefferedLightingRenderer::DefferedLightingRenderer()
{
	auto model = AssetManager::getModelAsset("assets/icosphere_3.fbx");
	icosphere_mesh = model->getRootNode()->children[0]->meshes[0];
}

DefferedLightingRenderer::~DefferedLightingRenderer()
{
}

void DefferedLightingRenderer::renderLights(FrameGraph &fg)
{
	struct LightingPassData
	{
		FrameGraphResource albedo;
		FrameGraphResource normal;
		FrameGraphResource depth;
		FrameGraphResource shading;
	};

	auto &gbuffer_data = fg.getBlackboard().get<GBufferData>();

	auto &lighting_data = fg.getBlackboard().add<DeferredLightingData>();

	auto *cascade_shadow_data = fg.getBlackboard().tryGet<CascadeShadowPass>();
	auto *ray_traced_shadow_data = fg.getBlackboard().tryGet<RayTracedShadowPass>();
	auto *shadow_passes_data = fg.getBlackboard().tryGet<ShadowPasses>();

	lighting_data = fg.addCallbackPass<DeferredLightingData>("Deffered Lighting Pass",
	[&](RenderPassBuilder &builder, DeferredLightingData &data)
	{
		// Setup
		FrameGraphTexture::Description desc;
		desc.width = Renderer::getViewportSize().x;
		desc.height = Renderer::getViewportSize().y;

		// Diffuse
		desc.format = FORMAT_R11G11B10_UFLOAT;
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
		desc.debug_name = "Lighting Diffuse Image";

		data.diffuse_light = builder.createResource<FrameGraphTexture>(desc.debug_name, desc);
		data.diffuse_light = builder.write(data.diffuse_light);

		// Specular
		desc.format = FORMAT_R11G11B10_UFLOAT;
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
		desc.debug_name = "Lighting Specular Image";

		data.specular_light = builder.createResource<FrameGraphTexture>(desc.debug_name, desc);
		data.specular_light = builder.write(data.specular_light);

		builder.read(gbuffer_data.albedo);
		builder.read(gbuffer_data.normal);
		builder.read(gbuffer_data.depth);
		builder.read(gbuffer_data.shading);

		if (cascade_shadow_data && cascade_shadow_data->shadow_map != -1)
			builder.read(cascade_shadow_data->shadow_map);

		if (shadow_passes_data)
		{
			for (auto &map : shadow_passes_data->shadow_maps)
				builder.read(map);
		}

		if (engine_ray_tracing && render_ray_traced_shadows && render_shadows && ray_traced_shadow_data)
		{
			builder.read(ray_traced_shadow_data->visibility);
		}
	},
	[=](const DeferredLightingData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		// Render
		auto &albedo = resources.getResource<FrameGraphTexture>(gbuffer_data.albedo);
		auto &normal = resources.getResource<FrameGraphTexture>(gbuffer_data.normal);
		auto &depth = resources.getResource<FrameGraphTexture>(gbuffer_data.depth);
		auto &shading = resources.getResource<FrameGraphTexture>(gbuffer_data.shading);

		auto &diffuse = resources.getResource<FrameGraphTexture>(data.diffuse_light);
		auto &specular = resources.getResource<FrameGraphTexture>(data.specular_light);

		cmd_list->setRenderTargets({diffuse.texture, specular.texture}, nullptr, -1, 0, true);

		ubo.albedo_tex_id = albedo.getBindlessId();
		ubo.normal_tex_id = normal.getBindlessId();
		ubo.depth_tex_id = depth.getBindlessId();
		ubo.shading_tex_id = shading.getBindlessId();

		// Render Lights radiance
		auto &p = gGlobalPipeline;
		p->reset();

		p->setRenderTargets(cmd_list->getCurrentRenderTargets());
		p->setUseBlending(true);
		p->setBlendMode(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD,
						VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD);
		p->setDepthTest(false);
		p->setCullMode(CULL_MODE_FRONT);


		if (engine_ray_tracing && render_ray_traced_shadows && render_shadows && ray_traced_shadow_data)
		{
			auto entities_id = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();
			LightComponent *light_component = nullptr;
			glm::vec3 dir;
			for (entt::entity entity_id : entities_id)
			{
				Entity entity(entity_id);
				auto &light = entity.getComponent<LightComponent>();

				if (light.getType() == LIGHT_TYPE_DIRECTIONAL)
				{
					light_component = &light;
					dir = entity.getLocalDirection(glm::vec3(0, 0, -1));
					break;
				}
			}

			if (light_component)
			{
				/*
				auto &visibility = resources.getResource<FrameGraphTexture>(ray_traced_shadow_data->visibility);

				// TODO: 
				p->setVertexShader(gDynamicRHI->createShader(L"shaders/lighting/deffered_lighting.vert", VERTEX_SHADER, L"????")); 
				////p->setFragmentShader(gDynamicRHI->createShader(L"shaders/lighting/deffered_lighting.frag", FRAGMENT_SHADER, {{"RAY_TRACED_SHADOWS", "1"}, {"USE_SHADOWS", render_shadows ? "1" : "0"}}));

				p->flush();
				p->bind(command_buffer);

				const auto uniforms = Renderer::getDefaultUniforms();
				ubo_sphere.model = glm::translate(glm::mat4(1), glm::vec3(uniforms.camera_position));

				constants.light_pos = glm::vec4(dir, 1.0);
				constants.light_color = glm::vec4(light_component->color, 1.0);
				constants.light_range_square = pow(light_component->radius, 2);
				constants.light_intensity = light_component->intensity;
				constants.z_far = light_component->radius;

				Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 1, &ubo, sizeof(UBO));
				Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo_sphere, sizeof(UniformBufferObject));

				Renderer::setShadersTexture(p->getCurrentShaders(), 2, visibility.texture);

				Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

				vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant), &constants);

				// Render mesh
				// TODO: fix
				/*
				VkBuffer vertexBuffers[] = {icosphere_mesh->vertexBuffer->buffer_handle};
				VkDeviceSize offsets[] = {0};
				vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(command_buffer.get_buffer(), icosphere_mesh->indexBuffer->buffer_handle, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(command_buffer.get_buffer(), icosphere_mesh->indices.size(), 1, 0, 0, 0);
				p->unbind(command_buffer);
				*/
			}
		} else
		{
			std::vector<std::pair<const char *, const char *>> shader_defines;

			auto entities_id = Scene::getCurrentScene()->getEntitiesWith<LightComponent>();
			for (entt::entity entity_id : entities_id)
			{
				Entity entity(entity_id);
				auto &light = entity.getComponent<LightComponent>();

				shader_defines.clear();
				shader_defines.push_back({"USE_SHADOWS", render_shadows ? "1" : "0"});
				shader_defines.push_back({"RAY_TRACED_SHADOWS", "0"});
				if (light.getType() == LIGHT_TYPE_POINT)
					shader_defines.push_back({"LIGHT_TYPE", "0"});
				else if (light.getType() == LIGHT_TYPE_DIRECTIONAL)
					shader_defines.push_back({"LIGHT_TYPE", "1"});

				p->setVertexShader(gDynamicRHI->createShader(L"shaders/lighting/deferred_lighting.hlsl", VERTEX_SHADER, shader_defines));
				p->setFragmentShader(gDynamicRHI->createShader(L"shaders/lighting/deferred_lighting.hlsl", FRAGMENT_SHADER, shader_defines));
				p->setVertexInputsDescription(Engine::Vertex::GetVertexInputsDescription());

				p->setRenderTargets(cmd_list->getCurrentRenderTargets());
				p->flush();
				p->bind(cmd_list);

				glm::vec3 scale, position, skew;
				glm::vec4 persp;
				glm::quat rotation;
				glm::decompose(entity.getWorldTransformMatrix(), scale, rotation, position, skew, persp);

				auto &transform = entity.getTransform();

				// Uniforms
				ubo_sphere.model = glm::translate(glm::mat4(1), position) *
					glm::scale(glm::mat4(1), glm::vec3(light.radius, light.radius, light.radius));

				if (light.type == LIGHT_TYPE_DIRECTIONAL)
				{
					const auto uniforms = Renderer::getDefaultUniforms();
					ubo_sphere.model = glm::translate(glm::mat4(1), glm::vec3(uniforms.camera_position));

					for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
					{
						ubo_sphere.cascade_splits[i] = light.cascades[i].splitDepth;
						ubo_sphere.light_matrix[i] = light.cascades[i].viewProjMatrix;
					}
					constants.light_pos = glm::vec4(entity.getLocalDirection(glm::vec3(0, 0, -1)), 1.0);
				} else if (light.type == LIGHT_TYPE_POINT)
				{
					constants.light_pos = glm::vec4(position, 1.0f);
				}

				constants.light_color = glm::vec4(light.color, 1.0);
				constants.light_range_square = pow(light.radius, 2);
				constants.light_intensity = light.intensity;
				constants.z_far = light.radius;

				gDynamicRHI->setConstantBufferData(1, &ubo, sizeof(UBO));
				gDynamicRHI->setConstantBufferData(0, &ubo_sphere, sizeof(UniformBufferObject));
				gDynamicRHI->setTexture(5, light.shadow_map);

				gDynamicRHI->setConstantBufferData(2, &constants, sizeof(PushConstant));

				// Render mesh
				cmd_list->setVertexBuffer(icosphere_mesh->vertexBuffer);
				cmd_list->setIndexBuffer(icosphere_mesh->indexBuffer);
				cmd_list->drawIndexedInstanced(icosphere_mesh->indices.size(), 1, 0, 0, 0);

				p->unbind(cmd_list);
				cmd_list->resetRenderTargets();
			}
		}
	});
}

void DefferedLightingRenderer::renderImgui()
{
}
