#include "pch.h"
#include "Device.h"
#include "Log.h"

static std::vector<const char*> s_ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
};

Device::Device(const VkInstance& instance) 
	: m_InstanceHandle(instance)
{
	CreatePhysicalDevice();
	CreateLogicalDevice();
}

Device::~Device()
{
	vkDestroyDevice(logicalHandle, nullptr);
}

void Device::CreatePhysicalDevice()
{
	uint32_t count;
	vkEnumeratePhysicalDevices(m_InstanceHandle, &count, nullptr);
	std::vector<VkPhysicalDevice> physicalDevices(count);
	vkEnumeratePhysicalDevices(m_InstanceHandle, &count, physicalDevices.data());
	CORE_INFO("GPUs Count: {}", count);

	// TODO: select device more smart
	physicalHandle = physicalDevices[0];

	uint32_t propertiesCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalHandle, &propertiesCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(propertiesCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalHandle, &propertiesCount, queueFamilies.data());

	CORE_INFO("Queue families Count: {}", propertiesCount);

	for (int i = 0; i < propertiesCount; i++)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			queueFamily.graphicsFamily = i;
			queueFamily.presentFamily = i;
		}

		// Its likely to have graphics and present in one queue
		if (queueFamily.graphicsFamily.has_value() && queueFamily.presentFamily.has_value())
		{
			break;
		}
	}
	CORE_INFO("Selected graphics queue id: {}", queueFamily.graphicsFamily.value());
	CORE_INFO("Selected present queue id: {}", queueFamily.presentFamily.value());

	vkGetPhysicalDeviceProperties(physicalHandle, &physicalProperties);
	vkGetPhysicalDeviceMemoryProperties(physicalHandle, &memory_properties);
}

void Device::CreateLogicalDevice()
{
	// Queues will be created with the logical device
	std::vector<VkDeviceQueueCreateInfo> queueInfos;
	// Set because there cant be equal values, only unique
	std::unordered_set<uint32_t> queues = { queueFamily.graphicsFamily.value(), queueFamily.presentFamily.value() };
	float priority = 1.0;
	for (auto queue : queues)
	{
		VkDeviceQueueCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		info.queueFamilyIndex = queue;
		info.queueCount = 1;
		info.pQueuePriorities = &priority;
		queueInfos.push_back(info);
	}

	VkDeviceCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	info.queueCreateInfoCount = queueInfos.size();
	info.pQueueCreateInfos = queueInfos.data();
	info.enabledLayerCount = s_ValidationLayers.size();
	info.ppEnabledLayerNames = s_ValidationLayers.data();

	std::vector<const char*> extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	info.enabledExtensionCount = extensions.size();
	info.ppEnabledExtensionNames = extensions.data();

	// Enable dynamic rendering
	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = true;
	/*
	VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature{};
	dynamic_rendering_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
	dynamic_rendering_feature.dynamicRendering = true;
	info.pNext = &dynamic_rendering_feature;
	*/

	VkPhysicalDeviceFeatures2 enabledFeatures{};
	enabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	enabledFeatures.features.samplerAnisotropy = true;
	enabledFeatures.pNext = &features13;
	info.pNext = &enabledFeatures;

	CHECK_ERROR(vkCreateDevice(physicalHandle, &info, nullptr, &logicalHandle));

	//Get queues from newly created device
	vkGetDeviceQueue(logicalHandle, queueFamily.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(logicalHandle, queueFamily.presentFamily.value(), 0, &presentQueue);
}
