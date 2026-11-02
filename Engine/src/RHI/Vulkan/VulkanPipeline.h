#pragma once
#include "RHI/RHIPipeline.h"
#include "VulkanResources.h"
#include "Descriptors.h"

class VulkanPipeline final: public RHIPipeline
{
public:
	VulkanPipeline(const PipelineDescription &description);
	~VulkanPipeline();

	std::unique_ptr<VkPipelineResource> resource;
	DescriptorLayout descriptor_layout;

	eastl::vector<Descriptor> descriptors;

	// TODO: refactor
	eastl::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups{};

	RHIBufferRef raygen_sbt;
	RHIBufferRef miss_sbt;
	RHIBufferRef hit_sbt;
};