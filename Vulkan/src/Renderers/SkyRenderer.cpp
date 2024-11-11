#include "pch.h"
#include "SkyRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Model.h"
#include "Utils/Math.h"
#include "Variables.h"
#include <imgui.h>

SkyRenderer::SkyRenderer(): RendererBase()
{
	TextureDescription tex_description{};
	tex_description.width = 1024;
	tex_description.height = 1024;
	tex_description.imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	tex_description.is_cube = true;

	cube_texture = std::make_shared<Texture>(tex_description);
	setMode(SKY_MODE_PROCEDURAL);

	auto model = AssetManager::getModelAsset("assets/cube.fbx");
	mesh = model->getRootNode()->children[0]->meshes[0];

	vertex_shader = Shader::create("shaders/cube.vert", Shader::VERTEX_SHADER);
	fragment_shader = Shader::create("shaders/cube.frag", Shader::FRAGMENT_SHADER);
}

SkyRenderer::~SkyRenderer()
{
}

void SkyRenderer::renderCubemap(CommandBuffer &command_buffer)
{
	if (mode != SKY_MODE_PROCEDURAL)
		return;

	if (!isDirty())
		return;
	prev_uniform = procedural_uniforms;

	GPU_SCOPE_FUNCTION(&command_buffer);
	struct PushConstant
	{
		glm::mat4 mvp = glm::mat4(1.0f);
	} constants;

	cube_texture->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);
	for (int face = 0; face < 6; face++)
	{
		VkWrapper::cmdBeginRendering(command_buffer, {cube_texture}, nullptr, face);
		auto &p = VkWrapper::global_pipeline;
		p->reset();

		p->setVertexShader(Shader::create("shaders/ibl/cubemap_filter.vert", Shader::VERTEX_SHADER));
		p->setFragmentShader(Shader::create("shaders/procedural_sky.frag", Shader::FRAGMENT_SHADER));

		p->setRenderTargets(VkWrapper::current_render_targets);
		p->setCullMode(VK_CULL_MODE_BACK_BIT);
		p->setDepthTest(false);

		p->flush();
		p->bind(command_buffer);

		// Uniforms
		Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &procedural_uniforms, sizeof(Uniforms));
		Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

		constants.mvp = glm::perspective(glm::radians(90.0f), 1.0f, 0.01f, 512.0f) * Math::getCubeFaceTransform(face);
		vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &constants);

		// Render mesh
		VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);

		p->unbind(command_buffer);

		VkWrapper::cmdEndRendering(command_buffer);
	}
	cube_texture->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
}

void SkyRenderer::fillCommandBuffer(CommandBuffer &command_buffer)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(Shader::create("shaders/cube.vert", Shader::VERTEX_SHADER));
	p->setFragmentShader(Shader::create("shaders/cube.frag", Shader::FRAGMENT_SHADER));
	p->setUseBlending(false);

	p->setRenderTargets(VkWrapper::current_render_targets);
	p->setCullMode(VK_CULL_MODE_FRONT_BIT);

	p->flush();
	p->bind(command_buffer);

	// Uniforms
	Renderer::setShadersTexture(p->getCurrentShaders(), 1, cube_texture);
	Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

	// Render mesh
	VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);
	p->unbind(command_buffer);
}

void SkyRenderer::renderImgui()
{
	ImGui::SliderFloat("Sun Dir X", &procedural_uniforms.sun_direction.x, -1.0, 1.0f);
	ImGui::SliderFloat("Sun Dir Y", &procedural_uniforms.sun_direction.y, -1.0, 1.0f);
	ImGui::SliderFloat("Sun Dir Z", &procedural_uniforms.sun_direction.z, -1.0, 1.0f);
}

void SkyRenderer::setMode(SKY_MODE mode)
{
	if (this->mode == mode)
		return;

	this->mode = mode;

	if (mode == SKY_MODE_CUBEMAP)
		cube_texture->load("assets/cubemap/");
	else
		cube_texture->fill();
}

bool SkyRenderer::isDirty()
{
	if (mode != SKY_MODE_PROCEDURAL)
		return false;
	bool dirty = false;
	if (prev_uniform.sun_direction != procedural_uniforms.sun_direction)
		dirty = true;
	if (render_first_frame)
		dirty = true;
	return dirty;
}
