#include "pch.h"
#include "DefferedLightingRenderer.h"
#include "imgui.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Scene/Entity.h"
#include "Model.h"

DefferedLightingRenderer::DefferedLightingRenderer()
{
	auto model = AssetManager::getAsset<Model>("assets/icosphere_3.fbx");
	icosphere_mesh = model->getRootNode()->children[0]->meshes[0];

	lighting_vertex_shader_point = Shader::create("shaders/lighting/deffered_lighting.vert", Shader::VERTEX_SHADER, {{"LIGHT_TYPE", "0"}});
	lighting_fragment_shader_point = Shader::create("shaders/lighting/deffered_lighting.frag", Shader::FRAGMENT_SHADER, {{"LIGHT_TYPE", "0"}});

	lighting_vertex_shader_directional = Shader::create("shaders/lighting/deffered_lighting.vert", Shader::VERTEX_SHADER, {{"LIGHT_TYPE", "1"}});
	lighting_fragment_shader_directional = Shader::create("shaders/lighting/deffered_lighting.frag", Shader::FRAGMENT_SHADER, {{"LIGHT_TYPE", "1"}});
}

DefferedLightingRenderer::~DefferedLightingRenderer()
{
}

void DefferedLightingRenderer::renderLights(Scene *scene, CommandBuffer &command_buffer, uint32_t image_index)
{
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


	auto entities_id = scene->getEntitiesWith<LightComponent>();
	for (entt::entity entity_id : entities_id)
	{
		Entity entity(entity_id, scene);
		auto &light = entity.getComponent<LightComponent>();

		if (light.getType() == LIGHT_TYPE_POINT)
		{
			p->setVertexShader(lighting_vertex_shader_point);
			p->setFragmentShader(lighting_fragment_shader_point);
		} else
		{
			p->setVertexShader(lighting_vertex_shader_directional);
			p->setFragmentShader(lighting_fragment_shader_directional);
		}
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
		} else
		{
			constants.light_pos = glm::vec4(position, 1.0f);
		}

		constants.light_color = glm::vec4(light.color, 1.0);
		constants.light_range_square = pow(light.radius, 2);
		constants.light_intensity = light.intensity;
		constants.z_far = light.radius;

		Renderer::setShadersUniformBuffer(p->getVertexShader(), p->getFragmentShader(), 1, &ubo, sizeof(UBO), image_index);
		Renderer::setShadersUniformBuffer(p->getVertexShader(), p->getFragmentShader(), 0, &ubo_sphere, sizeof(UniformBufferObject), image_index);
		Renderer::setShadersTexture(p->getVertexShader(), p->getFragmentShader(), 2, light.shadow_map, image_index);

		Renderer::bindShadersDescriptorSets(p->getVertexShader(), p->getFragmentShader(), command_buffer, p->getPipelineLayout(), image_index);

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

void DefferedLightingRenderer::renderImgui()
{
	for (int i = 0; i < lights.size(); i++)
	{
		LightData &light = lights[i];
		ImGui::PushID(i);
		static int present_mode = light.position.w;
		char *items[] = {"Point", "Directional"};
		if (ImGui::BeginCombo("Light type", items[(int)light.position.w]))
		{
			for (int n = 0; n < IM_ARRAYSIZE(items); n++)
			{
				bool is_selected = (light.position.w == n);
				if (ImGui::Selectable(items[n], is_selected))
					light.position.w = n;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		//light.position.w = present_mode;
		ImGui::SliderFloat3("Light Position", light.position.data.data, -8, 8);
		ImGui::SliderFloat3("Light Color", light.color.data.data, 0, 1);
		ImGui::SliderFloat("Light Radius", &light.radius, 0.001f, 25);
		ImGui::SliderFloat("Light Intensity", &light.intensity, 0.01f, 25);
		ImGui::Separator();
		ImGui::PopID();
	}
}
