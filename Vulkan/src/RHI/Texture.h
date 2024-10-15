#pragma once
#include "vma/vk_mem_alloc.h"
#include "Log.h"
#include "Device.h"
#include <yaml-cpp/yaml.h>
#include "Assets/Asset.h"

struct TextureDescription
{
	bool is_cube = false;
	uint32_t width;
	uint32_t height;
	uint32_t mipLevels = 1;
	uint32_t arrayLevels = 1;
	VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT;
	VkFormat imageFormat;
	VkImageAspectFlags imageAspectFlags;
	VkImageUsageFlags imageUsageFlags;
	VkSamplerAddressMode sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkFilter filtering = VK_FILTER_LINEAR;
	bool anisotropy = false;

	bool destroy_image = true; // primary used for swapchain images
};

class CommandBuffer;

enum TextureLayoutType
{
	TEXTURE_LAYOUT_UNDEFINED,
	TEXTURE_LAYOUT_GENERAL,
	TEXTURE_LAYOUT_ATTACHMENT,
	TEXTURE_LAYOUT_SHADER_READ,
	TEXTURE_LAYOUT_TRANSFER_SRC,
	TEXTURE_LAYOUT_TRANSFER_DST,
};

class Texture : public Asset
{
public:
	VkBuffer bufferHandle = nullptr;
	VmaAllocation allocation = nullptr;
	VkImage imageHandle = nullptr;
	VkSampler sampler = nullptr;
public:
	Texture(TextureDescription description);
	virtual ~Texture();

	ASSET_TYPE getAssetType() const override { return ASSET_TYPE_TEXTURE; }

	void cleanup();
	void fill();
	void fill(const void* sourceData);
	void load(const char* path);

	void fill_raw(VkImage image);

	std::string getPath() const { return path; }
	uint32_t getWidth(int mip = 0) const { return m_Description.width >> mip; }
	uint32_t getHeight(int mip = 0) const { return m_Description.height >> mip; }

	void transitLayout(CommandBuffer &command_buffer, TextureLayoutType new_layout_type, int mip = -1);
	
	VkImageView getImageView(int mip = 0, int layer = -1);

	VkFormat getImageFormat() const { return m_Description.imageFormat; }
	VkImageUsageFlags getImageUsageFlags() const { return m_Description.imageUsageFlags; }

	bool isCompressedFormat(VkFormat format) const
	{ 
		return
			format == VK_FORMAT_BC1_RGB_UNORM_BLOCK ||
			format == VK_FORMAT_BC3_UNORM_BLOCK ||
			format == VK_FORMAT_BC5_UNORM_BLOCK ||
			format == VK_FORMAT_BC7_UNORM_BLOCK;
	}

	bool isDepthTexture() const { return m_Description.imageAspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT; }
	void generate_mipmaps(CommandBuffer &command_buffer);
private:
	VkImageLayout get_vk_layout(TextureLayoutType layout_type);
	int get_channels_count() const;
	int get_bytes_per_channel() const;
	VkDeviceSize get_image_size() const;

	void copy_buffer_to_image(CommandBuffer &command_buffer, VkBuffer buffer);
	void create_sampler();

	struct ImageView
	{
		void cleanup();

		int mip = 0;
		int layer = 0;
		// ImageView needs to gain some information about how to render into this image
		VkImageView image_view = nullptr;
	};
private:
	std::vector<ImageView> image_views;
	TextureDescription m_Description;
	std::vector<TextureLayoutType> current_layouts; // Image layouts for each mip map
	std::string path = "";
};
