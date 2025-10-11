#pragma once
#include "RHI/RHIPipeline.h"
#include "VulkanResources.h"

class VulkanPipeline final: public RHIPipeline
{
public:
	~VulkanPipeline();

	void destroy();
	
	void create(const PipelineDescription &description) override;

	PipelineDescription description;

	std::unique_ptr<VkPipelineResource> resource;
	DescriptorLayout descriptor_layout;

	eastl::vector<Descriptor> descriptors;

	// TODO: refactor
	eastl::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};

	RHIBufferRef raygenShaderBindingTable;
	RHIBufferRef missShaderBindingTable;
	RHIBufferRef hitShaderBindingTable;
};