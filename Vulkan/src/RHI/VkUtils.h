#pragma once
#include "vulkan/vulkan.h"

struct CommandBuffer;

class VkUtils
{
public:
	static void init();

	static void setDebugName(VkObjectType object_type, uint64_t handle, const char *name);
	static void cmdBeginDebugLabel(CommandBuffer &command_buffer, const char *name, glm::vec3 color);
	static void cmdEndDebugLabel(CommandBuffer &command_buffer);

    static VkDeviceAddress vkGetBufferDeviceAddressKHR(const VkBufferDeviceAddressInfo *pInfo);

    static void vkCmdBuildAccelerationStructuresKHR(
        VkCommandBuffer commandBuffer,
        uint32_t infoCount,
        const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
        const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos);

    static void vkBuildAccelerationStructuresKHR(
        VkDeferredOperationKHR deferredOperation,
        uint32_t infoCount,
        const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
        const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos);

    static VkResult vkCreateAccelerationStructureKHR(
        const VkAccelerationStructureCreateInfoKHR *pCreateInfo,
        const VkAllocationCallbacks *pAllocator,
        VkAccelerationStructureKHR *pAccelerationStructure);

    static void vkDestroyAccelerationStructureKHR(
        VkAccelerationStructureKHR accelerationStructure,
        const VkAllocationCallbacks *pAllocator);

    static void vkGetAccelerationStructureBuildSizesKHR(
        VkAccelerationStructureBuildTypeKHR buildType,
        const VkAccelerationStructureBuildGeometryInfoKHR *pBuildInfo,
        const uint32_t *pMaxPrimitiveCounts,
        VkAccelerationStructureBuildSizesInfoKHR *pSizeInfo);

    static VkDeviceAddress vkGetAccelerationStructureDeviceAddressKHR(
        const VkAccelerationStructureDeviceAddressInfoKHR *pInfo);

    static VkResult vkGetRayTracingShaderGroupHandlesKHR(
        VkPipeline pipeline,
        uint32_t firstGroup,
        uint32_t groupCount,
        size_t dataSize,
        void *pData);

    static VkResult vkCreateRayTracingPipelinesKHR(
        VkDeferredOperationKHR deferredOperation,
        VkPipelineCache pipelineCache,
        uint32_t createInfoCount,
        const VkRayTracingPipelineCreateInfoKHR *pCreateInfos,
        const VkAllocationCallbacks *pAllocator,
        VkPipeline *pPipelines);

    static void vkCmdTraceRaysKHR(
        VkCommandBuffer commandBuffer,
        const VkStridedDeviceAddressRegionKHR *pRaygenShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR *pMissShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR *pHitShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR *pCallableShaderBindingTable,
        uint32_t width,
        uint32_t height,
        uint32_t depth);
	
private:
	inline static PFN_vkSetDebugUtilsObjectNameEXT p_vkSetDebugUtilsObjectNameEXT;
	inline static PFN_vkCmdBeginDebugUtilsLabelEXT p_vkCmdBeginDebugUtilsLabelEXT;
	inline static PFN_vkCmdEndDebugUtilsLabelEXT p_vkCmdEndDebugUtilsLabelEXT;

    inline static PFN_vkGetBufferDeviceAddressKHR p_vkGetBufferDeviceAddressKHR;

	inline static PFN_vkCmdBuildAccelerationStructuresKHR p_vkCmdBuildAccelerationStructuresKHR;
	inline static PFN_vkBuildAccelerationStructuresKHR p_vkBuildAccelerationStructuresKHR;
	inline static PFN_vkCreateAccelerationStructureKHR p_vkCreateAccelerationStructureKHR;
	inline static PFN_vkDestroyAccelerationStructureKHR p_vkDestroyAccelerationStructureKHR;
	inline static PFN_vkGetAccelerationStructureBuildSizesKHR p_vkGetAccelerationStructureBuildSizesKHR;
	inline static PFN_vkGetAccelerationStructureDeviceAddressKHR p_vkGetAccelerationStructureDeviceAddressKHR;
	inline static PFN_vkGetRayTracingShaderGroupHandlesKHR p_vkGetRayTracingShaderGroupHandlesKHR;
	inline static PFN_vkCreateRayTracingPipelinesKHR p_vkCreateRayTracingPipelinesKHR;

	inline static PFN_vkCmdTraceRaysKHR p_vkCmdTraceRaysKHR;
};