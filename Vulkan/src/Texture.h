#pragma once
#include "Log.h"
#include "Device.h"

struct TextureDescription
{
	uint32_t width;
	uint32_t height;
	uint32_t mipLevels = 1;
	VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT;
	VkFormat imageFormat;
	VkImageAspectFlags imageAspectFlags;
	VkImageUsageFlags imageUsageFlags;
};

class Texture
{
public:
	VkBuffer bufferHandle;
	VkDeviceMemory memoryHandle;
	VkImage imageHandle;
	VkImageView imageView;
	VkSampler sampler = nullptr;
public:
	Texture(TextureDescription description);
	virtual ~Texture();
	void fill();
	void fill(const void* sourceData);
	void load(const char* path);
private:
	void generate_mipmaps();
	void transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void create_image_view();
	void create_sampler();
private:
	TextureDescription m_Description;
};

