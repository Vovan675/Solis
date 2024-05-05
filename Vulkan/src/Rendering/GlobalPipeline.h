#pragma once
#include "MathUtils.h"
#include "RHI/Pipeline.h"
#include "RHI/CommandBuffer.h"
#include "RHI/Texture.h"

class GlobalPipeline
{
public:
	GlobalPipeline();

	void reset();
	void flush();

	void setVertexShader(std::shared_ptr<Shader> shader) { current_description.vertex_shader = shader; }
	void setFragmentShader(std::shared_ptr<Shader> shader) { current_description.fragment_shader = shader; }

	void setUseVertices(bool use_vertices) { current_description.use_vertices = use_vertices; }
	void setUseBlending(bool use_blending) { current_description.use_blending = use_blending; }
	void setDepthTest(bool depth_test) { current_description.use_depth_test = depth_test; }

	void setCullMode(VkCullModeFlagBits cull_mode) { current_description.cull_mode = cull_mode; }

	void setDescriptorLayout(DescriptorLayout layout) { current_description.descriptor_layout = layout; }
	void setColorFormats(std::vector<VkFormat> color_formats) { current_description.color_formats = color_formats; }
	void setRenderTargets(std::vector<std::shared_ptr<Texture>> color_attachments, std::shared_ptr<Texture> depth_attachment)
	{ 
		current_description.color_formats.clear();
		for (const auto &color : color_attachments)
			current_description.color_formats.push_back(color->getImageFormat());

		if (depth_attachment)
			current_description.depth_format = depth_attachment->getImageFormat();
	}

	void bind(const CommandBuffer &command_buffer);
	void unbind(const CommandBuffer& command_buffer);

	VkPipelineLayout getPipelineLayout() const { return current_pipeline->pipeline_layout; }
private:
	void parse_descriptors();

private:
	bool is_binded = false;
	PipelineDescription current_description;
	std::shared_ptr<Pipeline> current_pipeline;
	std::unordered_map<size_t, std::shared_ptr<Pipeline>> cached_pipelines;
};

