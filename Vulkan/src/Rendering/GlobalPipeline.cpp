#include "pch.h"
#include "GlobalPipeline.h"

GlobalPipeline::GlobalPipeline()
{
	reset();
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

	setupPushConstantRanges();

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

void GlobalPipeline::bind(const CommandBuffer &command_buffer)
{
	vkCmdBindPipeline(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, current_pipeline->pipeline);
	is_binded = true;
}

void GlobalPipeline::unbind(const CommandBuffer & command_buffer)
{
	is_binded = false;
}

void GlobalPipeline::setupPushConstantRanges()
{
	auto vs_descriptors = current_description.vertex_shader->getDescriptors();
	auto fs_descriptors = current_description.fragment_shader->getDescriptors();

	auto result_descriptors = vs_descriptors;
	
	for (int i = 0; i < fs_descriptors.size(); i++)
	{
		auto& other = fs_descriptors[i];

		bool is_found = false;

		for (auto &result : result_descriptors)
		{
			// If this descriptor exists in other descriptor, then just merge their stages
			if (result.set == other.set && result.binding == other.binding && result.first_member_offset == other.first_member_offset)
			{
				result.stage = (DescriptorStage)(result.stage | other.stage);
				is_found = true;
				break;
			}
		}
		
		// This is unique descriptor
		if (!is_found)
		{
			result_descriptors.push_back(other);
		}
	}

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
		VkPushConstantRange range;
		range.stageFlags = stage_to_vk(descriptor.stage);
		range.offset = descriptor.first_member_offset;
		range.size = descriptor.size;
		ranges.push_back(range);
	}
	current_description.push_constant_ranges = ranges;
}
