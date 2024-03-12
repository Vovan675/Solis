#pragma once
#include "Shader.h"

struct PipelineDescription
{
	std::shared_ptr<Shader> vertex_shader;
	std::shared_ptr<Shader> fragment_shader;
	bool use_vertices = true;
	VkDescriptorSetLayout descriptor_set_layout;
	std::vector<VkFormat> color_formats;
	VkFormat depth_format;
};

class Pipeline
{
public:

	virtual ~Pipeline();

	void cleanup();
	void create(const PipelineDescription &description);
public:
	VkPipeline pipeline = nullptr;
	VkPipelineLayout pipeline_layout = nullptr;
};

