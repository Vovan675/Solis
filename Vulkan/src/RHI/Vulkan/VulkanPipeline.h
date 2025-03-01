#pragma once
#include "RHI/RHIPipeline.h"

class VulkanPipeline : public RHIPipeline
{
public:
	~VulkanPipeline();

	void destroy() override;
	
	void create(const PipelineDescription &description) override;

	PipelineDescription description;

	VkPipeline pipeline = nullptr;
	VkPipelineLayout pipeline_layout = nullptr;
	DescriptorLayout descriptor_layout;

	std::vector<Descriptor> descriptors;
};