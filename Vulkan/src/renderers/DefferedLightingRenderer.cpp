#include "pch.h"
#include "DefferedLightingRenderer.h"
#include "imgui.h"
#include "BindlessResources.h"

DefferedLightingRenderer::DefferedLightingRenderer()
{
	// Create uniform buffers
	VkDeviceSize bufferSize = sizeof(UBO);

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
	descriptor_set_layout = layout_builder.build(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	// Create descriptor set
	descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		descriptor_sets[i] = VkWrapper::global_descriptor_allocator->allocate(descriptor_set_layout);
	}

	// Update descriptor set
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		DescriptorWriter writer;
		writer.writeBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniform_buffers[i]->bufferHandle, sizeof(UBO));
		writer.updateSet(descriptor_sets[i]);
	}

	recreatePipeline();
}

DefferedLightingRenderer::~DefferedLightingRenderer()
{
	vkDestroyDescriptorSetLayout(VkWrapper::device->logicalHandle, descriptor_set_layout, nullptr);
}

void DefferedLightingRenderer::recreatePipeline()
{
	// Create pipeline
	auto vertShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/quad.vert", Shader::VERTEX_SHADER);
	auto fragShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/deffered_lighting.frag", Shader::FRAGMENT_SHADER);

	PipelineDescription description{};
	description.vertex_shader = vertShader;
	description.fragment_shader = fragShader;
	description.use_vertices = false;
	description.use_depth_test = false;
	description.use_blending = false;
	
	VkPushConstantRange push_constant_range{};
	push_constant_range.offset = 0;
	push_constant_range.size = sizeof(PushConstant);
	push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	description.push_constant_ranges = {push_constant_range};

	description.color_formats = {VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_B10G11R11_UFLOAT_PACK32};
	description.descriptor_set_layout = descriptor_set_layout;

	pipeline = std::make_shared<Pipeline>();
	pipeline->create(description);
}

void DefferedLightingRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	vkCmdBindPipeline(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, 1, &descriptor_sets[image_index], 0, nullptr);

	vkCmdPushConstants(command_buffer.get_buffer(), pipeline->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant), &constants);

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);
}

void DefferedLightingRenderer::updateUniformBuffer(uint32_t image_index)
{
	memcpy(uniform_buffers_mapped[image_index], &ubo, sizeof(ubo));
}

void DefferedLightingRenderer::renderImgui()
{

}
