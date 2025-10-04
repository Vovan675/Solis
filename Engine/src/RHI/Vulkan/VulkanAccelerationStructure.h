#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include "RHI/RHIAccelerationStructure.h"
#include "RHI/Vulkan/VulkanUtils.h"
#include "Utils/Math.h"

class VulkanBottomLevelAccelerationStructure final: public RHIBottomLevelAccelerationStructure
{
public:
	~VulkanBottomLevelAccelerationStructure()
	{
		if (handle)
			VulkanUtils::vkDestroyAccelerationStructureKHR(handle, nullptr);
	}

	void build(const std::vector<RayTracingGeometry> &geometries) override;

	VkAccelerationStructureKHR handle = nullptr;
	uint64_t deviceAddress = 0;
	VkDeviceMemory memory;
	RHIBufferRef buffer;
	RHIBufferRef scratch_buffer;
};


class VulkanTopLevelAccelerationStructure final: public RHITopLevelAccelerationStructure
{
public:
	~VulkanTopLevelAccelerationStructure()
	{
		if (handle)
			VulkanUtils::vkDestroyAccelerationStructureKHR(handle, nullptr);
	}

	void build(bool update, const std::vector<RayTracingInstance> &instances) override;

	VkAccelerationStructureKHR handle = 0;
	uint64_t deviceAddress = 0;
	VkDeviceMemory memory;
	RHIBufferRef acc_buffer;
	RHIBufferRef scratch_buffer;
};