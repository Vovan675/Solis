#pragma once
#include "vma/vk_mem_alloc.h"
#include "Log.h"
#include "Device.h"

struct TextureDescription
{
	bool is_cube = false;
	uint32_t width;
	uint32_t height;
	uint32_t mipLevels = 1;
	VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT;
	VkFormat imageFormat;
	VkImageAspectFlags imageAspectFlags;
	VkImageUsageFlags imageUsageFlags;

	bool destroy_image = true; // primary used for swapchain images
};

class Texture
{
public:
	VkBuffer bufferHandle = nullptr;
	VmaAllocation allocation = nullptr;
	VkImage imageHandle = nullptr;
	// ImageView needs to gain some information about how to render into this image
	VkImageView imageView = nullptr;
	VkSampler sampler = nullptr;
public:
	Texture(TextureDescription description);
	virtual ~Texture();
	void cleanup();
	void fill();
	void fill(const void* sourceData);
	void load(const char* path);

	void fill_raw(VkImage image);

	uint32_t getWidth() const { return m_Description.width; }
	uint32_t getHeight() const { return m_Description.height; }
private:
	void generate_mipmaps();
	void transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void create_image_view();
	void create_sampler();
private:
	TextureDescription m_Description;
};

