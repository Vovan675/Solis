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
	vkDestroyQueryPool(logicalHandle, query_pool, nullptr);
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


	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	// TODO: enable bindless
	//features12.bufferDeviceAddress = true;
	//features12.descriptorIndexing = true;
	features12.descriptorBindingPartiallyBound = true;
	features12.descriptorBindingVariableDescriptorCount = true;
	features12.descriptorBindingSampledImageUpdateAfterBind = true;
	features12.runtimeDescriptorArray = true; // for GL_EXT_nonuniform_qualifier extension
	features12.hostQueryReset = true;

	// Enable dynamic rendering
	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = true;
	features13.synchronization2 = true;
	features13.pNext = &features12;

	VkPhysicalDeviceFeatures2 enabledFeatures{};
	enabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	enabledFeatures.features.samplerAnisotropy = true;
	enabledFeatures.pNext = &features13;
	info.pNext = &enabledFeatures;

	CHECK_ERROR(vkCreateDevice(physicalHandle, &info, nullptr, &logicalHandle));

	//Get queues from newly created device
	vkGetDeviceQueue(logicalHandle, queueFamily.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(logicalHandle, queueFamily.presentFamily.value(), 0, &presentQueue);

	// Create query pool for profiling
	VkQueryPoolCreateInfo query_pool_info{};
	query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
	query_pool_info.queryCount = time_stamps.size();
	vkCreateQueryPool(logicalHandle, &query_pool_info, nullptr, &query_pool);
	time_stamps.fill(0);
}
