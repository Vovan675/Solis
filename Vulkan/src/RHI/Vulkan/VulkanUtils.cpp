#include "pch.h"
#include "VulkanUtils.h"
#include "VulkanDynamicRHI.h"

void VulkanUtils::init()
{
	auto device = getNativeRHI()->device->logicalHandle;

	p_vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
	p_vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
	p_vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));

	p_vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(getNativeRHI()->device->logicalHandle, "vkGetBufferDeviceAddressKHR"));

	p_vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
	p_vkBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkBuildAccelerationStructuresKHR"));
	p_vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
	p_vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
	p_vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
	p_vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));

	p_vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
	p_vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));

	p_vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
}

inline VulkanDynamicRHI *VulkanUtils::getNativeRHI() { return (VulkanDynamicRHI *)gDynamicRHI; }

void VulkanUtils::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize dst_offset)
{
	// Also i can create separate command pool for these short-living commands
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = getNativeRHI()->command_pool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	CHECK_ERROR(vkAllocateCommandBuffers(getNativeRHI()->device->logicalHandle, &allocInfo, &commandBuffer));

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = dst_offset;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	vkQueueSubmit(getNativeRHI()->device->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(getNativeRHI()->device->graphicsQueue);

	vkFreeCommandBuffers(getNativeRHI()->device->logicalHandle, getNativeRHI()->command_pool, 1, &commandBuffer);
}

void VulkanUtils::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage, VkBuffer &buffer, VmaAllocation &allocation, VkDeviceSize alignment)
{
	// Create buffer
	VkBufferCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size = size;
	info.usage = usage;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo alloc_info{};
	alloc_info.usage = memory_usage;

	if (alignment != 0)
		vmaCreateBufferWithAlignment(getNativeRHI()->allocator, &info, &alloc_info, alignment, &buffer, &allocation, nullptr);
	else
		vmaCreateBuffer(getNativeRHI()->allocator, &info, &alloc_info, &buffer, &allocation, nullptr);
}

void VulkanUtils::cmdImageMemoryBarrier(VkCommandBuffer cmd_buffer, VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask, VkImageLayout old_layout, VkImageLayout new_layout, VkImage image, VkImageAspectFlags aspect_mask, int level_count, int layer_count, int base_level, int base_layer)
{
	VkImageMemoryBarrier2 image_memory_barrier{};
	image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	image_memory_barrier.srcStageMask = src_stage_mask; // pipeline stage(s) that must be completed before the barrier is crossed
	image_memory_barrier.srcAccessMask = src_access_mask; // operations that must complete before the barrier is crossed - example: write
	image_memory_barrier.dstStageMask = dst_stage_mask; // pipeline stage(s) that must wait for the barrier to be crossed before beginning
	image_memory_barrier.dstAccessMask = dst_access_mask; // operations that must wait for the barrier to be is crossed - example: read
	image_memory_barrier.oldLayout = old_layout;
	image_memory_barrier.newLayout = new_layout;
	image_memory_barrier.image = image;
	image_memory_barrier.subresourceRange.aspectMask = aspect_mask;
	image_memory_barrier.subresourceRange.baseMipLevel = base_level;
	image_memory_barrier.subresourceRange.levelCount = level_count;
	image_memory_barrier.subresourceRange.baseArrayLayer = base_layer;
	image_memory_barrier.subresourceRange.layerCount = layer_count;

	VkDependencyInfo dependency_info{};
	dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency_info.imageMemoryBarrierCount = 1;
	dependency_info.pImageMemoryBarriers = &image_memory_barrier;

	vkCmdPipelineBarrier2(cmd_buffer, &dependency_info);
}

void VulkanUtils::setDebugName(VkObjectType object_type, uint64_t handle, const char *name)
{
	VkDebugUtilsObjectNameInfoEXT nameInfo = {};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = object_type;
	nameInfo.objectHandle = handle;
	nameInfo.pObjectName = name;
	p_vkSetDebugUtilsObjectNameEXT(getNativeRHI()->device->logicalHandle, &nameInfo);
}

void VulkanUtils::cmdBeginDebugLabel(RHICommandList *cmd_list, const char *name, glm::vec3 color)
{
	VkDebugUtilsLabelEXT label = {};
	label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	label.pLabelName = name;
	label.color[0] = color.r;
	label.color[1] = color.g;
	label.color[2] = color.b;
	label.color[3] = 1.0f;
	VulkanCommandList *native_cmd_list = (VulkanCommandList *)cmd_list;
	p_vkCmdBeginDebugUtilsLabelEXT(native_cmd_list->cmd_buffer, &label);
}

void VulkanUtils::cmdEndDebugLabel(RHICommandList *cmd_list)
{
	VulkanCommandList *native_cmd_list = (VulkanCommandList *)cmd_list;
	p_vkCmdEndDebugUtilsLabelEXT(native_cmd_list->cmd_buffer);
}

VkDeviceAddress VulkanUtils::vkGetBufferDeviceAddressKHR(const VkBufferDeviceAddressInfo *pInfo)
{
	return p_vkGetBufferDeviceAddressKHR(getNativeRHI()->device->logicalHandle, pInfo);
}

void VulkanUtils::vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR *pInfos, const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos)
{
	p_vkCmdBuildAccelerationStructuresKHR(commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
}

void VulkanUtils::vkBuildAccelerationStructuresKHR(VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR *pInfos, const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos)
{
	p_vkBuildAccelerationStructuresKHR(getNativeRHI()->device->logicalHandle, deferredOperation, infoCount, pInfos, ppBuildRangeInfos);
}

VkResult VulkanUtils::vkCreateAccelerationStructureKHR(const VkAccelerationStructureCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkAccelerationStructureKHR *pAccelerationStructure)
{
	return p_vkCreateAccelerationStructureKHR(getNativeRHI()->device->logicalHandle, pCreateInfo, pAllocator, pAccelerationStructure);
}

void VulkanUtils::vkDestroyAccelerationStructureKHR(VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks *pAllocator)
{
	p_vkDestroyAccelerationStructureKHR(getNativeRHI()->device->logicalHandle, accelerationStructure, pAllocator);
}

void VulkanUtils::vkGetAccelerationStructureBuildSizesKHR(VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR *pBuildInfo, const uint32_t *pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR *pSizeInfo)
{
	p_vkGetAccelerationStructureBuildSizesKHR(getNativeRHI()->device->logicalHandle, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
}

VkDeviceAddress VulkanUtils::vkGetAccelerationStructureDeviceAddressKHR(const VkAccelerationStructureDeviceAddressInfoKHR *pInfo)
{
	return p_vkGetAccelerationStructureDeviceAddressKHR(getNativeRHI()->device->logicalHandle, pInfo);
}

VkResult VulkanUtils::vkGetRayTracingShaderGroupHandlesKHR(VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void *pData)
{
	return p_vkGetRayTracingShaderGroupHandlesKHR(getNativeRHI()->device->logicalHandle, pipeline, firstGroup, groupCount, dataSize, pData);
}

VkResult VulkanUtils::vkCreateRayTracingPipelinesKHR(VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
	return p_vkCreateRayTracingPipelinesKHR(getNativeRHI()->device->logicalHandle, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

void VulkanUtils::vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth)
{
	p_vkCmdTraceRaysKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
}
