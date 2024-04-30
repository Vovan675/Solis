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
