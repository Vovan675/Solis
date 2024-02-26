#include "pch.h"
#include "Swapchain.h"
#include "Log.h"
#include "VkWrapper.h"

Swapchain::Swapchain(VkSurfaceKHR surface)
	: swapchainHandle(nullptr), surface(surface)
{

}

Swapchain::~Swapchain()
{
	Cleanup();
	vkDestroySurfaceKHR(VkWrapper::instance, surface, nullptr);
}

void Swapchain::Cleanup()
{
	if (swapchainImageViews.size() != 0)
	{
		for (auto view : swapchainImageViews)
		{
			vkDestroyImageView(VkWrapper::device->logicalHandle, view, nullptr);
		}
		swapchainImageViews.clear();
	}

	if (swapchainHandle != 0)
	{
		vkDestroySwapchainKHR(VkWrapper::device->logicalHandle, swapchainHandle, nullptr);
		swapchainHandle = nullptr;
	}
}

void Swapchain::Create(int width, int height)
{
	Cleanup();
	CreateSwapchain(width, height);
	CreateImageViews();
}

void Swapchain::CreateSwapchain(int width, int height)
{
	VkBool32 surfaceSupport;
	CHECK_ERROR(vkGetPhysicalDeviceSurfaceSupportKHR(VkWrapper::device->physicalHandle, VkWrapper::device->queueFamily.graphicsFamily.value(), surface, &surfaceSupport));
	if (!surfaceSupport)
	{
		CORE_CRITICAL("Surface doesn't support");
	}


	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkWrapper::device->physicalHandle, surface, &surfaceCapabilities);

	uint32_t surfaceFormatsCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(VkWrapper::device->physicalHandle, surface, &surfaceFormatsCount, nullptr);

	std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(VkWrapper::device->physicalHandle, surface, &surfaceFormatsCount, surfaceFormats.data());

	uint32_t presentModesCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(VkWrapper::device->physicalHandle, surface, &presentModesCount, nullptr);

	std::vector<VkPresentModeKHR> presentModes(presentModesCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(VkWrapper::device->physicalHandle, surface, &presentModesCount, presentModes.data());

	if (surfaceFormatsCount == 0 || presentModesCount == 0)
	{
		CORE_CRITICAL("Surface formats or Present modes not found");
	}

	//Choose best format and present mode
	for (const auto& format : surfaceFormats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			surfaceFormat = format;
		}
	}

	VkPresentModeKHR presentMode;
	for (const auto& mode : presentModes)
	{
		// vsync
		if (mode == VK_PRESENT_MODE_FIFO_KHR)
		{
			presentMode = mode;
		}
	}

	swapExtent = { (uint32_t)width, (uint32_t)height };
	swapExtent.width = std::clamp(swapExtent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
	swapExtent.height = std::clamp(swapExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);

	uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount != 0 && surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = surfaceCapabilities.minImageCount;
	}

	VkSwapchainCreateInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = surface;
	info.minImageCount = surfaceCapabilities.minImageCount;
	info.imageFormat = surfaceFormat.format;
	info.imageColorSpace = surfaceFormat.colorSpace;
	info.imageExtent = swapExtent;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = nullptr;

	info.preTransform = surfaceCapabilities.currentTransform;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.presentMode = presentMode;
	info.clipped = VK_TRUE;
	info.oldSwapchain = VK_NULL_HANDLE;

	CHECK_ERROR(vkCreateSwapchainKHR(VkWrapper::device->logicalHandle, &info, nullptr, &swapchainHandle));

	//Get swap chain images
	uint32_t imagesCount;
	vkGetSwapchainImagesKHR(VkWrapper::device->logicalHandle, swapchainHandle, &imagesCount, nullptr);
	swapchainImages.resize(imagesCount);
	vkGetSwapchainImagesKHR(VkWrapper::device->logicalHandle, swapchainHandle, &imagesCount, swapchainImages.data());
}

void Swapchain::CreateImageViews()
{
	swapchainImageViews.resize(swapchainImages.size());
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		VkImageViewCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.image = swapchainImages[i];
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info.format = surfaceFormat.format;
		info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		info.subresourceRange.baseMipLevel = 0;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = 1;
		CHECK_ERROR(vkCreateImageView(VkWrapper::device->logicalHandle, &info, nullptr, &swapchainImageViews[i]));
	}
}
