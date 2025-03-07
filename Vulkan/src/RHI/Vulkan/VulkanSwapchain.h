#pragma once
#include "Device.h"
#include "RHI/RHISwapchain.h"

class VulkanSwapchain : public RHISwapchain
{
public:
	VulkanSwapchain(VkSurfaceKHR surface, const SwapchainInfo &info);
	virtual ~VulkanSwapchain();
	void cleanup();

	RHITextureRef getTexture(uint8_t index) override;
	void resize(uint32_t width, uint32_t height) override;
private:
	void create_swapchain();
	void create_resources();
private:
	friend class VulkanDynamicRHI;

	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain = nullptr;
	VkExtent2D swap_extent;
	std::vector<VkImage> swapchain_images;
	std::vector<Ref<RHITexture>> swapchain_textures;
};

