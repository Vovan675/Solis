#include "pch.h"
#include "GlobalPipeline.h"


GlobalPipeline::GlobalPipeline()
{
	reset();
}

GlobalPipeline::~GlobalPipeline()
{
	current_pipeline = nullptr;
	cached_pipelines.clear();
}

void GlobalPipeline::reset()
{
	current_description = PipelineDescription {};
}

void GlobalPipeline::flush()
{
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

void GlobalPipeline::setBlendMode(Blend src_color_blend, Blend dst_color_blend, BlendOp color_blend_op,
								  Blend src_alpha_blend, Blend dst_alpha_blend, BlendOp alpha_blend_op)
{
	current_description.src_color_blend = src_color_blend;
	current_description.dst_color_blend = dst_color_blend;
	current_description.color_blend_op = color_blend_op;
	current_description.src_alpha_blend = src_alpha_blend;
	current_description.dst_alpha_blend = dst_alpha_blend;
	current_description.alpha_blend_op = alpha_blend_op;
}

void GlobalPipeline::bindScreenQuadPipeline(RHICommandList *cmd_list, RHIShaderRef fragment_shader)
{
	setupGraphicsPipeline(cmd_list,
						 gDynamicRHI->createShader(L"shaders/quad.hlsl", VERTEX_SHADER),
						 fragment_shader,
						 VertexInputsDescription{},
						 false, // no blending
						 false, // no depth test
						 CULL_MODE_BACK);
	flushAndBind(cmd_list);
}

void GlobalPipeline::bind(RHICommandList *cmd_list)
{
	cmd_list->setPipeline(current_pipeline);
}

void GlobalPipeline::setupRayTracing(const wchar_t* shader_path)
{
	reset();
	current_description.pipeline_type = PipelineType::RayTracing;
	setRayGenerationShader(gDynamicRHI->createShader(shader_path, RAY_GENERATION_SHADER));
	setMissShader(gDynamicRHI->createShader(shader_path, MISS_SHADER));
	setClosestHitShader(gDynamicRHI->createShader(shader_path, CLOSEST_HIT_SHADER));
}

void GlobalPipeline::setupRayTracing(RHIShaderRef ray_gen, RHIShaderRef miss, RHIShaderRef closest_hit)
{
	reset();
	current_description.pipeline_type = PipelineType::RayTracing;
	setRayGenerationShader(ray_gen);
	setMissShader(miss);
	setClosestHitShader(closest_hit);
}

void GlobalPipeline::setupGraphicsPipeline(RHIShaderRef vertex_shader, RHIShaderRef fragment_shader,
										   const eastl::vector<RHITexture*>& render_targets,
										   VertexInputsDescription vertex_inputs,
										   bool use_blending,
										   bool depth_test,
										   CullMode cull_mode)
{
	reset();
	current_description.pipeline_type = PipelineType::Graphics;
	setVertexShader(vertex_shader);
	setFragmentShader(fragment_shader);
	if (!vertex_inputs.inputs.empty())
		setVertexInputsDescription(vertex_inputs);
	setRenderTargets(render_targets);
	setUseBlending(use_blending);
	setDepthTest(depth_test);
	setCullMode(cull_mode);
}

void GlobalPipeline::setupGraphicsPipeline(RHICommandList* cmd_list,
										   RHIShaderRef vertex_shader, RHIShaderRef fragment_shader,
										   VertexInputsDescription vertex_inputs,
										   bool use_blending,
										   bool depth_test,
										   CullMode cull_mode)
{
	setupGraphicsPipeline(vertex_shader, fragment_shader, cmd_list->getCurrentRenderTargets(),
						  vertex_inputs, use_blending, depth_test, cull_mode);
}

void GlobalPipeline::setupComputePipeline(RHIShaderRef compute_shader)
{
	reset();
	current_description.pipeline_type = PipelineType::Compute;
	setComputeShader(compute_shader);
}

void GlobalPipeline::setRenderTargets(eastl::vector<RHITexture *> attachments)
{
	current_description.color_formats.clear();
	current_description.depth_format = FORMAT_UNDEFINED;
	current_description.use_depth_test = false;
	for (const auto &attachment : attachments)
	{
		Format format = attachment->getDescription().format;
		if (attachment->isDepthTexture())
		{
			current_description.depth_format = format;
			current_description.use_depth_test = true;
		} else
		{
			current_description.color_formats.push_back(format);
		}
	}
}

void GlobalPipeline::setRenderTargets(RHICommandList* cmd_list)
{
	setRenderTargets(cmd_list->getCurrentRenderTargets());
}

void GlobalPipeline::flushAndBind(RHICommandList *cmd_list)
{
	flush();
	bind(cmd_list);
}