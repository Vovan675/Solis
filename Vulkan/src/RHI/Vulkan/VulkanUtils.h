#pragma once
#include "vulkan/vulkan.h"
#include "RHI/RHIDefinitions.h"
#include "RHI/RHIShader.h"
#include "RHI/RHICommandList.h"
#include "Descriptors.h"
#include "vma/vk_mem_alloc.h"

class VulkanDynamicRHI;

class VulkanUtils
{
public:
	static void init();

	static VulkanDynamicRHI *getNativeRHI();

	static VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer)
	{
		VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
		bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bufferDeviceAI.buffer = buffer;
		return VulkanUtils::vkGetBufferDeviceAddressKHR(&bufferDeviceAI);
	}

	static void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize dst_offset = 0);

	static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage, VkBuffer &buffer, VmaAllocation &allocation, VkDeviceSize alignment = 0);


	static void cmdImageMemoryBarrier(VkCommandBuffer cmd_buffer,
									  VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
									  VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask,
									  VkImageLayout old_layout, VkImageLayout new_layout,
									  VkImage image, VkImageAspectFlags aspect_mask,
									  int level_count = 1, int layer_count = 1,
									  int base_level = 0, int base_layer = 0);

	static DescriptorLayout getDescriptorLayout(std::vector<Descriptor> descriptors)
	{
		DescriptorLayout result;
		DescriptorLayoutBuilder layout_builder;
		layout_builder.clear();

		for (const auto &descriptor : descriptors)
		{
			// Only non specialized spaces allowed
			if (descriptor.set != 0)
				continue;

			// Skip bindless textures
			/*
			if ((descriptor.type == DESCRIPTOR_TYPE_SAMPLER && descriptor.set == 1 && descriptor.binding == 0) // Bindless Textures
			|| (descriptor.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER && descriptor.set == 2 && descriptor.binding == 0) // Default uniforms
			)
			continue;
			*/

			VkDescriptorType descriptor_type;
			if (descriptor.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
				descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			else if (descriptor.type == DESCRIPTOR_TYPE_STORAGE_BUFFER)
				descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			else if (descriptor.type == DESCRIPTOR_TYPE_SAMPLER)
				descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLER;
			else if (descriptor.type == DESCRIPTOR_TYPE_SAMPLED_IMAGE)
				descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			else if (descriptor.type == DESCRIPTOR_TYPE_COMBINED_IMAGE)
				descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			else if (descriptor.type == DESCRIPTOR_TYPE_STORAGE_IMAGE)
				descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			else if (descriptor.type == DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE)
				descriptor_type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
			else
				continue;
			layout_builder.add_binding(descriptor.binding, descriptor_type);
		}
		return layout_builder.build(VK_SHADER_STAGE_ALL);
	}

	static VkFormat getNativeFormat(Format format)
	{
		switch (format)
		{
			// 8 bit
			case FORMAT_R8_UNORM: return VK_FORMAT_R8_UNORM;
			case FORMAT_R8G8_UNORM: return VK_FORMAT_R8G8_UNORM;
			case FORMAT_R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
			case FORMAT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;

			// 16 bit
			case FORMAT_R16_UNORM: return VK_FORMAT_R16_UNORM;
			case FORMAT_R16G16_UNORM: return VK_FORMAT_R16G16_UNORM;
			case FORMAT_R16G16_SFLOAT: return VK_FORMAT_R16G16_SFLOAT;
			case FORMAT_R16G16B16A16_UNORM: return VK_FORMAT_R16G16B16A16_UNORM;

			// 32 bit
			case FORMAT_R32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
			case FORMAT_R32G32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
			case FORMAT_R32G32B32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
			case FORMAT_R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;

				// depth stencil
			case FORMAT_D32S8: return VK_FORMAT_D32_SFLOAT_S8_UINT;

			// combined
			case FORMAT_R11G11B10_UFLOAT: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;

			// compressed
			case FORMAT_BC1: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
			case FORMAT_BC3: return VK_FORMAT_BC3_UNORM_BLOCK;
			case FORMAT_BC5: return VK_FORMAT_BC5_UNORM_BLOCK;
			case FORMAT_BC7: return VK_FORMAT_BC7_UNORM_BLOCK;
		}
	}

	static uint32_t getFormatSize(Format format)
	{
		switch (format)
		{
			// 8 bit
			case FORMAT_R8_UNORM: return 1;
			case FORMAT_R8G8_UNORM: return 2;
			case FORMAT_R8G8B8A8_UNORM: return 3;

				// 16 bit
			case FORMAT_R16_UNORM: return 2;
			case FORMAT_R16G16_UNORM: return 4;
			case FORMAT_R16G16B16A16_UNORM: return 8;

				// 32 bit
			case FORMAT_R32_SFLOAT: return 4;
			case FORMAT_R32G32_SFLOAT: return 8;
			case FORMAT_R32G32B32_SFLOAT: return 16;
			case FORMAT_R32G32B32A32_SFLOAT: return 16;

				// depth stencil
			case FORMAT_D32S8: return 5;

				// combined
			case FORMAT_R11G11B10_UFLOAT: return 4;

				// compressed
			case FORMAT_BC1: return 0;
			case FORMAT_BC3: return 0;
			case FORMAT_BC5: return 0;
			case FORMAT_BC7: return 0;
		}
	}


	static void setDebugName(VkObjectType object_type, uint64_t handle, const char *name);
	static void cmdBeginDebugLabel(RHICommandList *cmd_list, const char *name, glm::vec3 color);
	static void cmdEndDebugLabel(RHICommandList *cmd_list);

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
	friend class VulkanDynamicRHI;

	inline static PFN_vkSetDebugUtilsObjectNameEXT p_vkSetDebugUtilsObjectNameEXT;
	inline static PFN_vkCmdBeginDebugUtilsLabelEXT p_vkCmdBeginDebugUtilsLabelEXT;
	inline static PFN_vkCmdEndDebugUtilsLabelEXT p_vkCmdEndDebugUtilsLabelEXT;

	inline static PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT p_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT;
	inline static PFN_vkGetCalibratedTimestampsEXT p_vkGetCalibratedTimestampsEXT;

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