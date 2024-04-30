#include "pch.h"
#include "DebugRenderer.h"
#include "BindlessResources.h"

DebugRenderer::DebugRenderer()
{
	// Create uniform buffers
	VkDeviceSize bufferSize = sizeof(PresentUBO);

	uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
	uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		BufferDescription desc;
		desc.size = bufferSize;
		desc.useStagingBuffer = false;
		desc.bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		uniform_buffers[i] = std::make_shared<Buffer>(desc);

		// Map gpu memory on cpu memory
		uniform_buffers[i]->map(&uniform_buffers_mapped[i]);
	}

	// Create descriptor set layout
	DescriptorLayoutBuilder layout_builder;
	layout_builder.clear();
	layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	descriptor_layout = layout_builder.build(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	// Create descriptor set
	descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		descriptor_sets[i] = VkWrapper::global_descriptor_allocator->allocate(descriptor_layout.layout);
	}

	// Update descriptor set
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		DescriptorWriter writer;
		writer.writeBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniform_buffers[i]->bufferHandle, sizeof(PresentUBO));
		writer.updateSet(descriptor_sets[i]);
	}

	reloadShaders();
}

DebugRenderer::~DebugRenderer()
{
	vkDestroyDescriptorSetLayout(VkWrapper::device->logicalHandle, descriptor_layout.layout, nullptr);
}

void DebugRenderer::reloadShaders()
{
	vertex_shader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/quad.vert", Shader::VERTEX_SHADER);
	fragment_shader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/debug_quad.frag", Shader::FRAGMENT_SHADER);
}

void DebugRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets, nullptr);
	p->setUseVertices(false);
	p->setUseBlending(false);
	p->setDescriptorLayout(descriptor_layout);

	p->flush();
	p->bind(command_buffer);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 0, 1, &descriptor_sets[image_index], 0, nullptr);

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
}

void DebugRenderer::updateUniformBuffer(uint32_t image_index)
{
	memcpy(uniform_buffers_mapped[image_index], &ubo, sizeof(ubo));
}
