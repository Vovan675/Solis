#include "pch.h"
#include "VkWrapper.h"

VkInstance VkWrapper::instance;
std::shared_ptr<Device> VkWrapper::device;
std::vector<CommandBuffer> VkWrapper::command_buffers;
std::shared_ptr<Swapchain> VkWrapper::swapchain;

namespace 
{
	VkCommandPool command_pool;
}


static std::vector<const char *> s_ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
};

void VkWrapper::init(GLFWwindow *window)
{
	init_instance();
	device = std::make_shared<Device>(instance);
	init_command_buffers();

	VkSurfaceKHR surface;
	CHECK_ERROR(glfwCreateWindowSurface(instance, window, nullptr, &surface));
	swapchain = std::make_shared<Swapchain>(surface);
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	swapchain->Create(width, height);
}

void VkWrapper::cleanup()
{
	vkDestroyCommandPool(device->logicalHandle, command_pool, nullptr);
}

void VkWrapper::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	// Also i can create separate command pool for these short-living commands
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = command_pool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	CHECK_ERROR(vkAllocateCommandBuffers(device->logicalHandle, &allocInfo, &commandBuffer));

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(device->graphicsQueue);

	vkFreeCommandBuffers(device->logicalHandle, command_pool, 1, &commandBuffer);
}

VkCommandBuffer VkWrapper::beginSingleTimeCommands()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = command_pool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device->logicalHandle, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void VkWrapper::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(device->graphicsQueue);

	vkFreeCommandBuffers(device->logicalHandle, command_pool, 1, &commandBuffer);
}

void VkWrapper::init_instance()
{
	uint32_t extensionsCount = 0;
	const char **extensionsName = glfwGetRequiredInstanceExtensions(&extensionsCount);

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Vulkan";
	appInfo.pEngineName = "Hello engine";
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.pApplicationInfo = &appInfo;
	info.enabledExtensionCount = extensionsCount;
	info.ppEnabledExtensionNames = extensionsName;


	info.enabledLayerCount = s_ValidationLayers.size();
	info.ppEnabledLayerNames = s_ValidationLayers.data();
	CHECK_ERROR(vkCreateInstance(&info, nullptr, &instance));
}

void VkWrapper::init_command_buffers()
{
	//Create Command Pool
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = device->queueFamily.graphicsFamily.value();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	CHECK_ERROR(vkCreateCommandPool(device->logicalHandle, &poolInfo, nullptr, &command_pool))

	//Create Command buffers
	command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < command_buffers.size(); i++)
	{
		command_buffers[i].init(device->logicalHandle, command_pool);
	}
}