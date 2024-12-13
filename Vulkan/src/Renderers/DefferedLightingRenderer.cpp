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

void DefferedLightingRenderer::renderLights(Scene *scene, CommandBuffer &command_buffer)
{
	ubo.albedo_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_ALBEDO);
	ubo.normal_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_NORMAL);
	ubo.depth_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_DEPTH_STENCIL);
	ubo.shading_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_SHADING);

	// Render Lights radiance
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setRenderTargets(VkWrapper::current_render_targets);
	p->setUseVertices(true);
	p->setUseBlending(true);
	p->setBlendMode(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD,
					VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD);
	p->setDepthTest(false);
	p->setCullMode(VK_CULL_MODE_FRONT_BIT);


	if (engine_ray_tracing && render_ray_traced_shadows && render_shadows)
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
			p->setVertexShader(Shader::create("shaders/lighting/deffered_lighting.vert", Shader::VERTEX_SHADER));
			p->setFragmentShader(Shader::create("shaders/lighting/deffered_lighting.frag", Shader::FRAGMENT_SHADER, {{"RAY_TRACED_SHADOWS", "1"}, {"USE_SHADOWS", render_shadows ? "1" : "0"}}));

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

			Renderer::setShadersTexture(p->getCurrentShaders(), 2, Renderer::getRenderTarget(RENDER_TARGET_RAY_TRACED_LIGHTING));

			Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

			vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant), &constants);

			// Render mesh
			VkBuffer vertexBuffers[] = {icosphere_mesh->vertexBuffer->bufferHandle};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(command_buffer.get_buffer(), icosphere_mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(command_buffer.get_buffer(), icosphere_mesh->indices.size(), 1, 0, 0, 0);
			p->unbind(command_buffer);
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

			if (light.getType() == LIGHT_TYPE_POINT)
				shader_defines.push_back({"LIGHT_TYPE", "0"});
			else if (light.getType() == LIGHT_TYPE_DIRECTIONAL)
				shader_defines.push_back({"LIGHT_TYPE", "1"});

			p->setVertexShader(Shader::create("shaders/lighting/deffered_lighting.vert", Shader::VERTEX_SHADER, shader_defines));
			p->setFragmentShader(Shader::create("shaders/lighting/deffered_lighting.frag", Shader::FRAGMENT_SHADER, shader_defines));

			p->flush();
			p->bind(command_buffer);

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

			Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 1, &ubo, sizeof(UBO));
			Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo_sphere, sizeof(UniformBufferObject));
			Renderer::setShadersTexture(p->getCurrentShaders(), 2, light.shadow_map);

			Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

			vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant), &constants);

			// Render mesh
			VkBuffer vertexBuffers[] = {icosphere_mesh->vertexBuffer->bufferHandle};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(command_buffer.get_buffer(), icosphere_mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(command_buffer.get_buffer(), icosphere_mesh->indices.size(), 1, 0, 0, 0);
			p->unbind(command_buffer);
		}
	}
}

void DefferedLightingRenderer::renderImgui()
{
}
