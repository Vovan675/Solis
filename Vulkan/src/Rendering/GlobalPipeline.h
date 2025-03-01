#pragma once
#include "Math.h"
#include "RHI/RHIPipeline.h"
#include "RHI/RHITexture.h"

class GlobalPipeline;
extern GlobalPipeline *gGlobalPipeline;

class GlobalPipeline
{
public:
	GlobalPipeline();
	~GlobalPipeline();

	void reset();
	void flush();

	void setVertexShader(std::shared_ptr<RHIShader> shader) { current_description.vertex_shader = shader; }
	void setFragmentShader(std::shared_ptr<RHIShader> shader) { current_description.fragment_shader = shader; }

	std::shared_ptr<RHIShader> getVertexShader() const { return current_description.vertex_shader; }
	std::shared_ptr<RHIShader> getFragmentShader() const { return current_description.fragment_shader; }

	void setVertexInputsDescription(VertexInputsDescription desc) { current_description.vertex_inputs_descriptions = desc; }

	void setUseBlending(bool use_blending) { current_description.use_blending = use_blending; }
	void setBlendMode(VkBlendFactor srcColorBlendFactor, VkBlendFactor dstColorBlendFactor, VkBlendOp colorBlendOp,
					  VkBlendFactor srcAlphaBlendFactor, VkBlendFactor dstAlphaBlendFactor, VkBlendOp alphaBlendOp);
	void setDepthTest(bool depth_test) { current_description.use_depth_test = depth_test; }

	void setCullMode(CullMode cull_mode) { current_description.cull_mode = cull_mode; }
	void setPrimitiveTopology(Topology topology) { current_description.primitive_topology = topology; }

	void setColorFormats(std::vector<Format> color_formats) { current_description.color_formats = color_formats; }
	void setRenderTargets(std::vector<std::shared_ptr<RHITexture>> attachments)
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

	void setIsComputePipeline(bool is_compute) { current_description.is_compute_pipeline = is_compute; }
	void setComputeShader(std::shared_ptr<RHIShader> shader) { current_description.compute_shader = shader; }

	void setIsRayTracingPipeline(bool is_ray_tracing) { current_description.is_ray_tracing_pipeline = is_ray_tracing; }

	void setRayGenerationShader(std::shared_ptr<RHIShader> shader) { current_description.ray_generation_shader = shader; }
	void setMissShader(std::shared_ptr<RHIShader> shader) { current_description.miss_shader = shader; }
	void setClosestHitShader(std::shared_ptr<RHIShader> shader) { current_description.closest_hit_shader = shader; }

	std::shared_ptr<RHIShader> getRayGenerationShader() const { return current_description.ray_generation_shader; }
	std::shared_ptr<RHIShader> getMissShader() const { return current_description.miss_shader; }
	std::shared_ptr<RHIShader> getClosestHitShader() const { return current_description.closest_hit_shader; }

	std::vector<std::shared_ptr<RHIShader>> getCurrentShaders();

	void bindScreenQuadPipeline(RHICommandList *cmd_list, std::shared_ptr<RHIShader> fragment_shader);

	void bind(RHICommandList *cmd_list);
	void unbind(RHICommandList *cmd_list);

public:
	bool is_binded = false;
	PipelineDescription current_description;
	std::shared_ptr<RHIPipeline> current_pipeline;
	std::unordered_map<size_t, std::shared_ptr<RHIPipeline>> cached_pipelines;
};

