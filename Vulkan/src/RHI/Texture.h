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

class CommandBuffer;

enum TextureLayoutType
{
	TEXTURE_LAYOUT_UNDEFINED,
	TEXTURE_LAYOUT_ATTACHMENT,
	TEXTURE_LAYOUT_SHADER_READ,
};

class Texture
{
public:
	VkBuffer bufferHandle = nullptr;
	VmaAllocation allocation = nullptr;
	VkImage imageHandle = nullptr;
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

	void transitLayout(CommandBuffer &command_buffer, TextureLayoutType new_layout_type);
	
	VkImageView getImageView(int mip = 0, int face = -1);
private:
	VkImageLayout get_vk_layout(TextureLayoutType layout_type);

	void generate_mipmaps();
	void transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void create_sampler();

	struct ImageView
	{
		void cleanup();

		int mip = 0;
		int face = 0;
		// ImageView needs to gain some information about how to render into this image
		VkImageView image_view = nullptr;
	};
private:
	std::vector<ImageView> image_views;
	TextureDescription m_Description;
	TextureLayoutType current_layout;
};

