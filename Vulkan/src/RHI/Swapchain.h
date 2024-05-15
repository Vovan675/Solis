#pragma once
#include "Device.h"
#include "Texture.h"

class Swapchain
{
public:
	VkExtent2D swap_extent;
	VkSwapchainKHR swapchain_handle = nullptr;
	std::vector<VkImage> swapchain_images;
	std::vector<std::shared_ptr<Texture>> swapchain_textures;
	VkSurfaceFormatKHR surface_format;
	VkSurfaceKHR surface;
public:
	Swapchain(VkSurfaceKHR surface);
	virtual ~Swapchain();
	void cleanup();
	void create(int width, int height);

	glm::vec2 getSize() const { return glm::vec2(swap_extent.width, swap_extent.height); }

private:
	void create_swapchain(int width, int height);
	void create_resources();
private:
};

