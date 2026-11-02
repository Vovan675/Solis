#include "pch.h"
#include "VulkanResources.h"
#include "VulkanDynamicRHI.h"
#include "VulkanUtils.h"

void VkPipelineResource::release()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	vkDestroyPipeline(native_rhi->device->logicalHandle, pipeline, nullptr);
	vkDestroyPipelineLayout(native_rhi->device->logicalHandle, pipeline_layout, nullptr);
}

void VkAllocationResource::release()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	vmaFreeMemory(native_rhi->allocator, resource);
}

void VkBufferResource::release()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	vkDestroyBuffer(native_rhi->device->logicalHandle, resource, nullptr);
}

void VkImageResource::release()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	vkDestroyImage(native_rhi->device->logicalHandle, resource, nullptr);
}

void VkImageViewResource::release()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	vkDestroyImageView(native_rhi->device->logicalHandle, resource, nullptr);
}

void VkSamplerResource::release()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	vkDestroySampler(native_rhi->device->logicalHandle, resource, nullptr);
}

void VkAccelerationStructureResource::release()
{
	VulkanUtils::vkDestroyAccelerationStructureKHR(resource, nullptr);
}
