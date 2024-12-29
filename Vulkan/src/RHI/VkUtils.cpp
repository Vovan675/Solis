#include "pch.h"
#include "VkUtils.h"
#include "VkWrapper.h"

void VkUtils::init()
{
	auto device = VkWrapper::device->logicalHandle;

	p_vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
	p_vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
	p_vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));

	p_vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(VkWrapper::device->logicalHandle, "vkGetBufferDeviceAddressKHR"));

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

void VkUtils::setDebugName(VkObjectType object_type, uint64_t handle, const char *name)
{
	VkDebugUtilsObjectNameInfoEXT nameInfo = {};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = object_type;
	nameInfo.objectHandle = handle;
	nameInfo.pObjectName = name;
	p_vkSetDebugUtilsObjectNameEXT(VkWrapper::device->logicalHandle, &nameInfo);
}

void VkUtils::cmdBeginDebugLabel(CommandBuffer &command_buffer, const char *name, glm::vec3 color)
{
	VkDebugUtilsLabelEXT label = {};
	label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	label.pLabelName = name;
	label.color[0] = color.r;
	label.color[1] = color.g;
	label.color[2] = color.b;
	label.color[3] = 1.0f;
	p_vkCmdBeginDebugUtilsLabelEXT(command_buffer.get_buffer(), &label);
}

void VkUtils::cmdEndDebugLabel(CommandBuffer &command_buffer)
{
	p_vkCmdEndDebugUtilsLabelEXT(command_buffer.get_buffer());
}

VkDeviceAddress VkUtils::vkGetBufferDeviceAddressKHR(const VkBufferDeviceAddressInfo *pInfo)
{
	return p_vkGetBufferDeviceAddressKHR(VkWrapper::device->logicalHandle, pInfo);
}

void VkUtils::vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR *pInfos, const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos)
{
	p_vkCmdBuildAccelerationStructuresKHR(commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
}

void VkUtils::vkBuildAccelerationStructuresKHR(VkDeferredOperationKHR deferredOperation, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR *pInfos, const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos)
{
	p_vkBuildAccelerationStructuresKHR(VkWrapper::device->logicalHandle, deferredOperation, infoCount, pInfos, ppBuildRangeInfos);
}

VkResult VkUtils::vkCreateAccelerationStructureKHR(const VkAccelerationStructureCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkAccelerationStructureKHR *pAccelerationStructure)
{
	return p_vkCreateAccelerationStructureKHR(VkWrapper::device->logicalHandle, pCreateInfo, pAllocator, pAccelerationStructure);
}

void VkUtils::vkDestroyAccelerationStructureKHR(VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks *pAllocator)
{
	p_vkDestroyAccelerationStructureKHR(VkWrapper::device->logicalHandle, accelerationStructure, pAllocator);
}

void VkUtils::vkGetAccelerationStructureBuildSizesKHR(VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR *pBuildInfo, const uint32_t *pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR *pSizeInfo)
{
	p_vkGetAccelerationStructureBuildSizesKHR(VkWrapper::device->logicalHandle, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
}

VkDeviceAddress VkUtils::vkGetAccelerationStructureDeviceAddressKHR(const VkAccelerationStructureDeviceAddressInfoKHR *pInfo)
{
	return p_vkGetAccelerationStructureDeviceAddressKHR(VkWrapper::device->logicalHandle, pInfo);
}

VkResult VkUtils::vkGetRayTracingShaderGroupHandlesKHR(VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void *pData)
{
	return p_vkGetRayTracingShaderGroupHandlesKHR(VkWrapper::device->logicalHandle, pipeline, firstGroup, groupCount, dataSize, pData);
}

VkResult VkUtils::vkCreateRayTracingPipelinesKHR(VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
{
	return p_vkCreateRayTracingPipelinesKHR(VkWrapper::device->logicalHandle, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

void VkUtils::vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth)
{
	p_vkCmdTraceRaysKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
}
