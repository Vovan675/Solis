#pragma once
#include "Math.h"
#include "RHI/Pipeline.h"
#include "RHI/CommandBuffer.h"
#include "RHI/Texture.h"

class GlobalPipeline
{
public:
	GlobalPipeline();
	~GlobalPipeline();

	void reset();
	void flush();

	void setVertexShader(std::shared_ptr<Shader> shader) { current_description.vertex_shader = shader; }
	void setFragmentShader(std::shared_ptr<Shader> shader) { current_description.fragment_shader = shader; }

	std::shared_ptr<Shader> getVertexShader() const { return current_description.vertex_shader; }
	std::shared_ptr<Shader> getFragmentShader() const { return current_description.fragment_shader; }

	void setUseVertices(bool use_vertices) { current_description.use_vertices = use_vertices; }
	void setUseBlending(bool use_blending) { current_description.use_blending = use_blending; }
	void setBlendMode(VkBlendFactor srcColorBlendFactor, VkBlendFactor dstColorBlendFactor, VkBlendOp colorBlendOp,
					  VkBlendFactor srcAlphaBlendFactor, VkBlendFactor dstAlphaBlendFactor, VkBlendOp alphaBlendOp);
	void setDepthTest(bool depth_test) { current_description.use_depth_test = depth_test; }

	void setCullMode(VkCullModeFlagBits cull_mode) { current_description.cull_mode = cull_mode; }
	void setPrimitiveTopology(VkPrimitiveTopology topology) { current_description.primitive_topology = topology; }

	void setDescriptorLayout(DescriptorLayout layout) { current_description.descriptor_layout = layout; }
	void setColorFormats(std::vector<VkFormat> color_formats) { current_description.color_formats = color_formats; }
	void setRenderTargets(std::vector<std::shared_ptr<Texture>> attachments)
	{ 
		current_description.color_formats.clear();
		current_description.depth_format = VK_FORMAT_UNDEFINED;
		for (const auto &attachment : attachments)
		{
			if (attachment->isDepthTexture())
				current_description.depth_format = attachment->getImageFormat();
			else
				current_description.color_formats.push_back(attachment->getImageFormat());
		}
	}

	void setIsComputePipeline(bool is_compute) { current_description.is_compute_pipeline = is_compute; }
	void setComputeShader(std::shared_ptr<Shader> shader) { current_description.compute_shader = shader; }

	void setIsRayTracingPipeline(bool is_ray_tracing) { current_description.is_ray_tracing_pipeline = is_ray_tracing; }

	void setRayGenerationShader(std::shared_ptr<Shader> shader) { current_description.ray_generation_shader = shader; }
	void setMissShader(std::shared_ptr<Shader> shader) { current_description.miss_shader = shader; }
	void setClosestHitShader(std::shared_ptr<Shader> shader) { current_description.closest_hit_shader = shader; }

	std::shared_ptr<Shader> getRayGenerationShader() const { return current_description.ray_generation_shader; }
	std::shared_ptr<Shader> getMissShader() const { return current_description.miss_shader; }
	std::shared_ptr<Shader> getClosestHitShader() const { return current_description.closest_hit_shader; }

	std::vector<std::shared_ptr<Shader>> getCurrentShaders();

	void bindScreenQuadPipeline(const CommandBuffer &command_buffer, std::shared_ptr<Shader> fragment_shader);

	void bind(const CommandBuffer &command_buffer);
	void unbind(const CommandBuffer& command_buffer);

	VkPipelineLayout getPipelineLayout() const { return current_pipeline->pipeline_layout; }
private:
	void parse_descriptors();

public:
	bool is_binded = false;
	PipelineDescription current_description;
	std::shared_ptr<Pipeline> current_pipeline;
	std::unordered_map<size_t, std::shared_ptr<Pipeline>> cached_pipelines;
};

