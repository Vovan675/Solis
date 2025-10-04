#include "pch.h"
#include "VulkanResources.h"
#include "VulkanDynamicRHI.h"
#include "VulkanUtils.h"

void VkPipelineResource::Release()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	vkDestroyPipeline(native_rhi->device->logicalHandle, pipeline, nullptr);
	vkDestroyPipelineLayout(native_rhi->device->logicalHandle, pipeline_layout, nullptr);
}

void VkAllocationResource::Release()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	vmaFreeMemory(native_rhi->allocator, resource);
}

void VkBufferResource::Release()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	vkDestroyBuffer(native_rhi->device->logicalHandle, resource, nullptr);
}

void VkImageResource::Release()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	vkDestroyImage(native_rhi->device->logicalHandle, resource, nullptr);
}

void VkImageViewResource::Release()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	vkDestroyImageView(native_rhi->device->logicalHandle, resource, nullptr);
}

void VkSamplerResource::Release()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	vkDestroySampler(native_rhi->device->logicalHandle, resource, nullptr);
}
