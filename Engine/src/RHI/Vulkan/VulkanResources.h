#pragma once
#include "RHI/RHIDefinitions.h"
#include "vma/vk_mem_alloc.h"

struct VkPipelineResource final: public RenderResource
{
	void Release() override;

	VkPipeline pipeline;
	VkPipelineLayout pipeline_layout;
};

struct VkAllocationResource final: public RenderResource
{
	void Release() override;

	VmaAllocation resource;
};

struct VkBufferResource final: public RenderResource
{
	void Release() override;

	VkBuffer resource;
};

struct VkImageResource final: public RenderResource
{
	void Release() override;

	VkImage resource;
};

struct VkImageViewResource final: public RenderResource
{
	void Release() override;

	VkImageView resource;
};

struct VkSamplerResource final: public RenderResource
{
	void Release() override;

	VkSampler resource;
};