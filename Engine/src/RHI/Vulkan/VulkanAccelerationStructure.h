#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include "RHI/RHIAccelerationStructure.h"
#include "RHI/Vulkan/VulkanUtils.h"
#include "RHI/Vulkan/VulkanResources.h"
#include "Utils/Math.h"

class VulkanBottomLevelAccelerationStructure final: public RHIBottomLevelAccelerationStructure
{
public:
	~VulkanBottomLevelAccelerationStructure();

	void build(const eastl::vector<RayTracingGeometry> &geometries) override;

	std::unique_ptr<VkAccelerationStructureResource> handle;
	uint64_t deviceAddress = 0;
	RHIBufferRef buffer;
};


class VulkanTopLevelAccelerationStructure final: public RHITopLevelAccelerationStructure
{
public:
	~VulkanTopLevelAccelerationStructure();

	void build(const eastl::vector<RayTracingInstance> &instances) override;
	uint32_t getBindlessId() override { return bindless_id; }

	std::unique_ptr<VkAccelerationStructureResource> handle;
	uint64_t deviceAddress = 0;
	RHIBufferRef acc_buffer;
	RHIBufferRef scratch_buffer;
	uint32_t bindless_id = 0;
};