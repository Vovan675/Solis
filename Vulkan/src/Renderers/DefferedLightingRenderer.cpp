#include "pch.h"
#include "DefferedLightingRenderer.h"
#include "imgui.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

DefferedLightingRenderer::DefferedLightingRenderer()
{
	icosphere_mesh = std::make_shared<Engine::Mesh>("assets/icosphere_3.fbx");

	LightData light;
	light.position = glm::vec4(0, 0, 0, 1);
	light.color = glm::vec4(1, 0, 0, 1);
	light.intensity = 1.0f;
	light.radius = 1.0f;
	lights.push_back(light);

	light.position.y = 0.3;
	light.color = glm::vec4(0, 1, 0, 1);
	lights.push_back(light);

	vertex_shader = Shader::create("shaders/lighting/deffered_lighting.vert", Shader::VERTEX_SHADER);
	fragment_shader = Shader::create("shaders/lighting/deffered_lighting.frag", Shader::FRAGMENT_SHADER);
}

DefferedLightingRenderer::~DefferedLightingRenderer()
{
}

void DefferedLightingRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets, nullptr);
	p->setUseVertices(true);
	p->setUseBlending(true);
	p->setBlendMode(VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, 
					VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD);
	p->setDepthTest(false);
	p->setCullMode(VK_CULL_MODE_FRONT_BIT);

	p->flush();
	p->bind(command_buffer);

	for (const auto& light : lights)
	{
		// Uniforms
		ubo_sphere.model = glm::translate(glm::mat4(1), glm::vec3(light.position)) *
			glm::scale(glm::mat4(1), glm::vec3(light.radius, light.radius, light.radius));
		constants.light_pos = light.position;
		constants.light_color = light.color;
		constants.light_range_square = pow(light.radius, 2);
		constants.light_intensity = light.intensity;

		Renderer::setShadersUniformBuffer(vertex_shader, fragment_shader, 1, &ubo, sizeof(UBO), image_index);
		Renderer::setShadersUniformBuffer(vertex_shader, fragment_shader, 0, &ubo_sphere, sizeof(UniformBufferObject), image_index);
		Renderer::bindShadersDescriptorSets(vertex_shader, fragment_shader, command_buffer, p->getPipelineLayout(), image_index);

		vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant), &constants);

		// Render mesh
		VkBuffer vertexBuffers[] = {icosphere_mesh->vertexBuffer->bufferHandle};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(command_buffer.get_buffer(), icosphere_mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(command_buffer.get_buffer(), icosphere_mesh->indices.size(), 1, 0, 0, 0);
	}
	p->unbind(command_buffer);
}

void DefferedLightingRenderer::renderImgui()
{
	for (int i = 0; i < lights.size(); i++)
	{
		LightData &light = lights[i];
		ImGui::PushID(i);
		ImGui::SliderFloat3("Light Position", light.position.data.data, -8, 8);
		ImGui::SliderFloat3("Light Color", light.color.data.data, 0, 1);
		ImGui::SliderFloat("Light Radius", &light.radius, 0.001f, 10);
		ImGui::SliderFloat("Light Intensity", &light.intensity, 0.01f, 10);
		ImGui::Separator();
		ImGui::PopID();
	}
}
