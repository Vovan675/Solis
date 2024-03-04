#pragma once
#include "Device.h"

class Swapchain
{
public:
	VkExtent2D swapExtent;
	VkSwapchainKHR swapchainHandle;
	std::vector<VkImage> swapchainImages;
	//ImageView needs gain some information about how to render into this image
	std::vector<VkImageView> swapchainImageViews;
	VkSurfaceFormatKHR surfaceFormat;
	VkSurfaceKHR surface;
public:
	Swapchain(VkSurfaceKHR surface);
	virtual ~Swapchain();
	void cleanup();
	void create(int width, int height);

private:
	void create_swapchain(int width, int height);
	void create_image_views();
private:
};

