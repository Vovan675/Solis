#pragma once
#include "Core/Core.h"

class Device : public RefCounted
{
public:
	VkPhysicalDeviceAccelerationStructurePropertiesKHR physicalAccelerationStructureProperties{};
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR physicalRayTracingProperties{};
	VkPhysicalDeviceProperties2 physicalProperties{};
	VkPhysicalDevice physicalHandle;
	VkPhysicalDeviceMemoryProperties memory_properties;
	VkDevice logicalHandle;
	struct
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;
	} queueFamily;

	VkQueue graphicsQueue;
	VkQueue presentQueue;
	std::vector<VkQueryPool> query_pools;
	std::vector<std::array<uint64_t, 256>> time_stamps;
public:
	Device(const VkInstance& instance);
	virtual ~Device();
private:
	void CreatePhysicalDevice();
	void CreateLogicalDevice();
private:
	const VkInstance& m_InstanceHandle;
};

