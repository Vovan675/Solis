#include "pch.h"
#include "GlobalPipeline.h"
#include "RHI/VkWrapper.h"

GlobalPipeline::GlobalPipeline()
{
	reset();
}

GlobalPipeline::~GlobalPipeline()
{
	cached_pipelines.clear();
}

void GlobalPipeline::reset()
{
	current_description = PipelineDescription {};
}

void GlobalPipeline::flush()
{
	if (is_binded)
	{
		CORE_CRITICAL("Can't flush pipeline while its binded");
		return;
	}

	parse_descriptors();

	// If hash the same, no need to change pipeline
	if (current_pipeline != nullptr && current_pipeline->getHash() == current_description.getHash())
	{
		return;
	}

	// Try to find cached pipeline
	auto cached_pipeline = cached_pipelines.find(current_description.getHash());
	if (cached_pipeline != cached_pipelines.end())
	{
		current_pipeline = cached_pipeline->second;
		return;
	}

	// Otherwise create new pipeline and cache it
	auto new_pipeline = std::make_shared<Pipeline>();
	new_pipeline->create(current_description);

	cached_pipelines[new_pipeline->getHash()] = new_pipeline;
	current_pipeline = new_pipeline;
}

void GlobalPipeline::setBlendMode(VkBlendFactor srcColorBlendFactor, VkBlendFactor dstColorBlendFactor, VkBlendOp colorBlendOp, VkBlendFactor srcAlphaBlendFactor, VkBlendFactor dstAlphaBlendFactor, VkBlendOp alphaBlendOp)
{
	current_description.srcColorBlendFactor = srcColorBlendFactor;
	current_description.dstColorBlendFactor = dstColorBlendFactor;
	current_description.colorBlendOp = colorBlendOp;
	current_description.srcAlphaBlendFactor = srcAlphaBlendFactor;
	current_description.dstAlphaBlendFactor = dstAlphaBlendFactor;
	current_description.alphaBlendOp = alphaBlendOp;
}

void GlobalPipeline::bind(const CommandBuffer &command_buffer)
{
	vkCmdBindPipeline(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipeline->pipeline);
	is_binded = true;
}

void GlobalPipeline::unbind(const CommandBuffer & command_buffer)
{
	is_binded = false;
}

void GlobalPipeline::parse_descriptors()
{
	auto result_descriptors = VkWrapper::getMergedDescriptors(current_description.vertex_shader, current_description.fragment_shader);

	// Create desciptor ranges based on descriptors
	const auto stage_to_vk = [](DescriptorStage stage) {
		VkShaderStageFlags vk_stage = 0;
		if (stage & DESCRIPTOR_STAGE_VERTEX_SHADER)
			vk_stage |= VK_SHADER_STAGE_VERTEX_BIT;
		if (stage & DESCRIPTOR_STAGE_FRAGMENT_SHADER)
			vk_stage |= VK_SHADER_STAGE_FRAGMENT_BIT;
		return vk_stage;
	};

	std::vector<VkPushConstantRange> ranges;
	for (const auto &descriptor : result_descriptors)
	{
		if (descriptor.type != DESCRIPTOR_TYPE_PUSH_CONSTANT)
			continue;
		VkPushConstantRange range;
		range.stageFlags = stage_to_vk(descriptor.stage);
		range.offset = descriptor.first_member_offset;
		range.size = descriptor.size;
		ranges.push_back(range);
	}
	current_description.push_constant_ranges = ranges;

	current_description.descriptor_layout = VkWrapper::getDescriptorLayout(result_descriptors);
}

