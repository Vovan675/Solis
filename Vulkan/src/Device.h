#pragma once

class Device
{
public:
	VkPhysicalDeviceProperties physicalProperties;
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
public:
	Device(const VkInstance& instance);
	virtual ~Device();
private:
	void CreatePhysicalDevice();
	void CreateLogicalDevice();
private:
	const VkInstance& m_InstanceHandle;
};

