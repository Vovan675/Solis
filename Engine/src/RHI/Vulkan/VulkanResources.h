#pragma once
#include "RHI/RHIDefinitions.h"
#include "vma/vk_mem_alloc.h"

struct VkPipelineResource final: public RenderResource
{
	void release() override;

	VkPipeline pipeline;
	VkPipelineLayout pipeline_layout;
};

struct VkAllocationResource final: public RenderResource
{
	void release() override;

	VmaAllocation resource;
};

struct VkBufferResource final: public RenderResource
{
	void release() override;

	VkBuffer resource;
};

struct VkImageResource final: public RenderResource
{
	void release() override;

	VkImage resource;
};

struct VkImageViewResource final: public RenderResource
{
	void release() override;

	VkImageView resource;
};

struct VkSamplerResource final: public RenderResource
{
	void release() override;

	VkSampler resource;
};

struct VkAccelerationStructureResource final: public RenderResource
{
	void release() override;

	VkAccelerationStructureKHR resource;
};