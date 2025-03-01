#include "pch.h"
#include "GlobalPipeline.h"


GlobalPipeline::GlobalPipeline()
{
	reset();
}

GlobalPipeline::~GlobalPipeline()
{
	cached_pipelines.clear();
	current_pipeline = nullptr;
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
	auto new_pipeline = gDynamicRHI->createPipeline();
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

std::vector<std::shared_ptr<RHIShader>> GlobalPipeline::getCurrentShaders()
{
	std::vector<std::shared_ptr<RHIShader>> shaders;
	if (current_description.vertex_shader)
		shaders.push_back(current_description.vertex_shader);
	if (current_description.fragment_shader)
		shaders.push_back(current_description.fragment_shader);
	if (current_description.compute_shader)
		shaders.push_back(current_description.compute_shader);
	if (current_description.ray_generation_shader)
		shaders.push_back(current_description.ray_generation_shader);
	if (current_description.miss_shader)
		shaders.push_back(current_description.miss_shader);
	if (current_description.closest_hit_shader)
		shaders.push_back(current_description.closest_hit_shader);
	return shaders;
}

void GlobalPipeline::bindScreenQuadPipeline(RHICommandList *cmd_list, std::shared_ptr<RHIShader> fragment_shader)
{
	reset();

	setVertexShader(gDynamicRHI->createShader(L"shaders/quad.hlsl", VERTEX_SHADER));
	setFragmentShader(fragment_shader);
	setRenderTargets(cmd_list->getCurrentRenderTargets());

	setUseBlending(false);
	setDepthTest(false);

	flush();
	bind(cmd_list);
}

void GlobalPipeline::bind(RHICommandList *cmd_list)
{
	cmd_list->setPipeline(current_pipeline);
	is_binded = true;
}

void GlobalPipeline::unbind(RHICommandList *cmd_list)
{
	is_binded = false;
}
