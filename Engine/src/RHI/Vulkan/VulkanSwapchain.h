#pragma once
#include "Device.h"
#include "RHI/RHISwapchain.h"

class VulkanSwapchain final: public RHISwapchain
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
	eastl::vector<VkImage> swapchain_images;
	eastl::vector<Ref<RHITexture>> swapchain_textures;
};

