#include "pch.h"
#include "CubeMapRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

CubeMapRenderer::CubeMapRenderer(std::shared_ptr<Camera> cam): RendererBase()
{
	camera = cam;

	// Load image
	TextureDescription tex_description{};
	tex_description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	tex_description.is_cube = true;
	cube_texture = std::make_shared<Texture>(tex_description);
	cube_texture->load("assets/cubemap/");

	camera = cam;
	mesh = std::make_shared<Engine::Mesh>("assets/cube.fbx");

	//mesh->setData(vertices, indices);

	reloadShaders();
}

CubeMapRenderer::~CubeMapRenderer()
{
}

void CubeMapRenderer::reloadShaders()
{
	vertex_shader = std::make_shared<Shader>("shaders/cube.vert", Shader::VERTEX_SHADER);
	fragment_shader = std::make_shared<Shader>("shaders/cube.frag", Shader::FRAGMENT_SHADER);
}

void CubeMapRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets, nullptr);
	p->setCullMode(VK_CULL_MODE_FRONT_BIT);

	p->flush();
	p->bind(command_buffer);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
	UniformBufferObject ubo{};
	ubo.view = camera->getView();
	ubo.proj = camera->getProj();
	ubo.camera_position = camera->getPosition();
	Renderer::setShadersUniformBuffer(vertex_shader, fragment_shader, 0, &ubo, sizeof(UniformBufferObject), image_index);
	Renderer::setShadersTexture(vertex_shader, fragment_shader, 1, cube_texture, image_index);
	Renderer::bindShadersDescriptorSets(vertex_shader, fragment_shader, command_buffer, p->getPipelineLayout(), image_index);

	// Render mesh
	VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);
	p->unbind(command_buffer);
}
