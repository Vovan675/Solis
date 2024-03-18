#include "pch.h"
#include "Swapchain.h"
#include "Log.h"
#include "VkWrapper.h"

Swapchain::Swapchain(VkSurfaceKHR surface): surface(surface)
{

}

Swapchain::~Swapchain()
{
	cleanup();
	vkDestroySurfaceKHR(VkWrapper::instance, surface, nullptr);
}

void Swapchain::cleanup()
{
	swapchain_textures.clear();

	if (swapchain_handle != 0)
	{
		vkDestroySwapchainKHR(VkWrapper::device->logicalHandle, swapchain_handle, nullptr);
		swapchain_handle = nullptr;
	}
}

void Swapchain::create(int width, int height)
{
	cleanup();
	create_swapchain(width, height);
	create_resources();
}

void Swapchain::create_swapchain(int width, int height)
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
			surface_format = format;
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

	swap_extent = { (uint32_t)width, (uint32_t)height };
	swap_extent.width = std::clamp(swap_extent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
	swap_extent.height = std::clamp(swap_extent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);

	uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount != 0 && surfaceCapabilities.maxImageCount < imageCount)
	{
		imageCount = surfaceCapabilities.minImageCount;
	}

	VkSwapchainCreateInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = surface;
	info.minImageCount = surfaceCapabilities.minImageCount;
	info.imageFormat = surface_format.format;
	info.imageColorSpace = surface_format.colorSpace;
	info.imageExtent = swap_extent;
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

	CHECK_ERROR(vkCreateSwapchainKHR(VkWrapper::device->logicalHandle, &info, nullptr, &swapchain_handle));
}

void Swapchain::create_resources()
{
	// Get swap chain images
	uint32_t imagesCount;
	vkGetSwapchainImagesKHR(VkWrapper::device->logicalHandle, swapchain_handle, &imagesCount, nullptr);
	swapchain_images.resize(imagesCount);
	vkGetSwapchainImagesKHR(VkWrapper::device->logicalHandle, swapchain_handle, &imagesCount, swapchain_images.data());

	// Create textures
	for (size_t i = 0; i < swapchain_images.size(); i++)
	{
		TextureDescription desc{};
		desc.width = swap_extent.width;
		desc.height = swap_extent.height;
		desc.imageFormat = surface_format.format;
		desc.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		desc.destroy_image = false; // dont destroy image because it will be destroyed with swapchain implicitly
		std::shared_ptr<Texture> tex = std::make_shared<Texture>(desc);
		tex->fill_raw(swapchain_images[i]);
		swapchain_textures.push_back(tex);
	}
}
