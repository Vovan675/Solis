#include "pch.h"
#include "IrradianceRenderer.h"
#include "BindlessResources.h"

IrradianceRenderer::IrradianceRenderer(): RendererBase()
{
	// Load image
	TextureDescription tex_description{};
	tex_description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	tex_description.is_cube = true;
	
	mesh = std::make_shared<Engine::Mesh>("assets/cube.fbx");

	//mesh->setData(vertices, indices);

	// Create uniform buffers
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	image_uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
	image_uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		BufferDescription desc;
		desc.size = bufferSize;
		desc.useStagingBuffer = false;
		desc.bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		image_uniform_buffers[i] = std::make_shared<Buffer>(desc);

		// Map gpu memory on cpu memory
		image_uniform_buffers[i]->map(&image_uniform_buffers_mapped[i]);
	}

	// Create descriptor set layout
	DescriptorLayoutBuilder layout_builder;
	layout_builder.clear();
	layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layout_builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	descriptor_layout = layout_builder.build(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	// Create descriptor set
	image_descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		image_descriptor_sets[i] = VkWrapper::global_descriptor_allocator->allocate(descriptor_layout.layout);
	}

	// Update descriptor set
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		DescriptorWriter writer;
		writer.writeBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, image_uniform_buffers[i]->bufferHandle, sizeof(UniformBufferObject));
		///writer.writeImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, tex_cube->imageView, tex_cube->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		writer.updateSet(image_descriptor_sets[i]);
	}

	reloadShaders();
}

IrradianceRenderer::~IrradianceRenderer()
{
	vkDestroyDescriptorSetLayout(VkWrapper::device->logicalHandle, descriptor_layout.layout, nullptr);
}

void IrradianceRenderer::reloadShaders()
{
	vertex_shader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/ibl/cubemap_filter.vert", Shader::VERTEX_SHADER);
	fragment_shader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/ibl/irradiance.frag", Shader::FRAGMENT_SHADER);
}

void IrradianceRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets, nullptr);
	p->setCullMode(VK_CULL_MODE_BACK_BIT);
	p->setPushConstantRanges({{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant)}});

	p->setDescriptorLayout(descriptor_layout);

	p->flush();
	p->bind(command_buffer);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 0, 1, &image_descriptor_sets[image_index], 0, nullptr);

	vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &constants);

	// Render mesh
	VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);

	p->unbind(command_buffer);
}

void IrradianceRenderer::updateUniformBuffer(uint32_t image_index)
{
	memcpy(image_uniform_buffers_mapped[image_index], &ubo, sizeof(ubo));

	// Update descriptor set
	DescriptorWriter writer;
	writer.writeImage(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, cube_texture->getImageView(), cube_texture->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	writer.updateSet(image_descriptor_sets[image_index]);
}