#include "pch.h"
#include "VulkanSwapchain.h"
#include "Log.h"
#include "Variables.h"
#include "VulkanUtils.h"
#include "VulkanDynamicRHI.h"

VulkanSwapchain::VulkanSwapchain(VkSurfaceKHR surface, const SwapchainInfo &info): surface(surface), RHISwapchain(info)
{
	create_swapchain();
	create_resources();
}

VulkanSwapchain::~VulkanSwapchain()
{
	cleanup();

	if (surface)
	{
		vkDestroySurfaceKHR(VulkanUtils::getNativeRHI()->instance, surface, nullptr);
		surface = nullptr;
	}
}

void VulkanSwapchain::cleanup()
{
	// dont destroy image because it will be destroyed with swapchain implicitly
	for (auto &tex : swapchain_textures)
	{
		VulkanTexture *native_texture = (VulkanTexture *)tex.getReference();
		native_texture->imageHandle = nullptr;
	}

	swapchain_textures.clear();

	if (swapchain != 0)
	{
		vkDestroySwapchainKHR(VulkanUtils::getNativeRHI()->device->logicalHandle, swapchain, nullptr);
		swapchain = nullptr;
	}
}

RHITextureRef VulkanSwapchain::getTexture(uint8_t index)
{
	return swapchain_textures[index];
}

void VulkanSwapchain::resize(uint32_t width, uint32_t height)
{
	info.width = width;
	info.height = height;

	cleanup();
	create_swapchain();
	create_resources();
}

void VulkanSwapchain::create_swapchain()
{
	auto &device = VulkanUtils::getNativeRHI()->device;

	VkBool32 surfaceSupport;
	CHECK_ERROR(vkGetPhysicalDeviceSurfaceSupportKHR(device->physicalHandle, device->queueFamily.graphicsFamily.value(), surface, &surfaceSupport));
	if (!surfaceSupport)
	{
		CORE_CRITICAL("Surface doesn't support");
	}

	VkSurfaceCapabilitiesKHR surface_capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physicalHandle, surface, &surface_capabilities);

	uint32_t surface_formats_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalHandle, surface, &surface_formats_count, nullptr);

	std::vector<VkSurfaceFormatKHR> surface_formats(surface_formats_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalHandle, surface, &surface_formats_count, surface_formats.data());

	uint32_t present_modes_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalHandle, surface, &present_modes_count, nullptr);

	std::vector<VkPresentModeKHR> present_modes(present_modes_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalHandle, surface, &present_modes_count, present_modes.data());

	if (surface_formats_count == 0 || present_modes_count == 0)
	{
		CORE_CRITICAL("Surface formats or Present modes not found");
	}

	//Choose best format and present mode
	VkSurfaceFormatKHR surface_format;
	for (const auto& format : surface_formats)
	{
		if (format.format == VulkanUtils::getNativeFormat(info.format))
		{
			surface_format = format;
		}
	}

	VkPresentModeKHR present_mode;
	for (const auto &mode : present_modes)
	{
		// vsync
		if (render_vsync)
		{
			if (mode == VK_PRESENT_MODE_FIFO_KHR)
				present_mode = mode;
		} else
		{
			if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
				present_mode = mode;
		}
	}

	swap_extent = { info.width, info.height };
	swap_extent.width = std::clamp(swap_extent.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
	swap_extent.height = std::clamp(swap_extent.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);

	VkSwapchainCreateInfoKHR create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = surface;
	create_info.minImageCount = info.textures_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = swap_extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_info.queueFamilyIndexCount = 0;
	create_info.pQueueFamilyIndices = nullptr;

	create_info.preTransform = surface_capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;

	CHECK_ERROR(vkCreateSwapchainKHR(device->logicalHandle, &create_info, nullptr, &swapchain));
}

void VulkanSwapchain::create_resources()
{
	auto &device = VulkanUtils::getNativeRHI()->device;

	// Get swap chain images
	uint32_t images_count;
	vkGetSwapchainImagesKHR(device->logicalHandle, swapchain, &images_count, nullptr);
	swapchain_images.resize(images_count);
	vkGetSwapchainImagesKHR(device->logicalHandle, swapchain, &images_count, swapchain_images.data());

	// Create textures
	for (size_t i = 0; i < swapchain_images.size(); i++)
	{
		TextureDescription desc{};
		desc.width = swap_extent.width;
		desc.height = swap_extent.height;
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
		desc.format = info.format;
		RHITextureRef tex = gDynamicRHI->createTexture(desc);
		tex->fill_raw(&swapchain_images[i]);
		tex->setDebugName("Swapchain Texture");
		swapchain_textures.push_back(tex);
	}
}
