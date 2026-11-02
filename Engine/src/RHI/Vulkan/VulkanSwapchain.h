#pragma once
#include "Device.h"
#include "RHI/RHISwapchain.h"

class VulkanSwapchain final: public RHISwapchain
{
public:
	VulkanSwapchain(VkSurfaceKHR surface, const SwapchainDescription &description);
	virtual ~VulkanSwapchain();
	void cleanup();

	void acquire() override;
	void present() override;
	void resize(uint32_t width, uint32_t height) override;

	// Use per frame in flight because image index is unknown before acquire
	VkSemaphore getImageAcquiredSemaphore() const { return image_acquired_semaphores[gDynamicRHI->getFrameInFlight()]; }

	// Use per real current index that was given by graphics api
	VkSemaphore getRenderingDoneSemaphore() const { return rendering_done_semaphores[current_index]; }

private:
	void create_swapchain();
	void create_resources();
	void release_resources();

private:
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain = nullptr;
	VkExtent2D swap_extent;
	eastl::vector<VkImage> swapchain_images;
	bool vsync = false;
	bool is_resize_needed = false;

	// Signaled when acquire. Waited by rendering commands submission. Lives always
	eastl::vector<VkSemaphore> image_acquired_semaphores;
	// Signaled by rendering commands submission. Waited by present. Lives with swap chain images
	eastl::vector<VkSemaphore> rendering_done_semaphores;
};
