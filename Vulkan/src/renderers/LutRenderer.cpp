#include "pch.h"
#include "LutRenderer.h"
#include "BindlessResources.h"

LutRenderer::LutRenderer(): RendererBase()
{
	// Create descriptor set layout
	DescriptorLayoutBuilder layout_builder;
	layout_builder.clear();
	descriptor_set_layout = layout_builder.build(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	recreatePipeline();
}

LutRenderer::~LutRenderer()
{
	vkDestroyDescriptorSetLayout(VkWrapper::device->logicalHandle, descriptor_set_layout, nullptr);
}

void LutRenderer::recreatePipeline()
{
	// Create pipeline
	auto vertShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/quad.vert", Shader::VERTEX_SHADER);
	auto fragShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/ibl/brdf_lut.frag", Shader::FRAGMENT_SHADER);

	PipelineDescription description{};
	description.vertex_shader = vertShader;
	description.fragment_shader = fragShader;
	description.use_vertices = false;
	description.use_blending = false;

	///description.descriptor_set_layout = descriptor_set_layout;
	description.color_formats = {VK_FORMAT_R16G16_SFLOAT};

	description.cull_mode = VK_CULL_MODE_BACK_BIT;

	pipeline = std::make_shared<Pipeline>();
	pipeline->create(description);
}

void LutRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	vkCmdBindPipeline(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);
}

void LutRenderer::updateUniformBuffer(uint32_t image_index)
{
}