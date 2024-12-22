#pragma once
#include "vma/vk_mem_alloc.h"
#include "Log.h"
#include "Device.h"
#include <yaml-cpp/yaml.h>
#include "Assets/Asset.h"
#include "GPUResourceManager.h"
#include "EngineMath.h"

enum TextureUsageFlags : uint32_t
{
	TEXTURE_USAGE_TRANSFER_SRC = 1 << 1,
	TEXTURE_USAGE_TRANSFER_DST = 1 << 2,
	TEXTURE_USAGE_NO_SAMPLED = 1 << 3,
	TEXTURE_USAGE_STORAGE = 1 << 4,
	TEXTURE_USAGE_ATTACHMENT = 1 << 5,
};

struct TextureDescription
{
	bool is_cube = false;
	uint32_t width;
	uint32_t height;
	uint32_t mip_levels = 1;
	uint32_t array_levels = 1;
	VkSampleCountFlagBits num_samples = VK_SAMPLE_COUNT_1_BIT;
	VkFormat image_format;
	uint32_t usage_flags = 0;
	VkSamplerAddressMode sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkFilter filtering = VK_FILTER_LINEAR;
	bool anisotropy = false;

	size_t getHash() const
	{
		size_t hash = 0;
		Engine::Math::hash_combine(hash, is_cube);
		Engine::Math::hash_combine(hash, width);
		Engine::Math::hash_combine(hash, height);
		Engine::Math::hash_combine(hash, mip_levels);
		Engine::Math::hash_combine(hash, array_levels);
		Engine::Math::hash_combine(hash, num_samples);
		Engine::Math::hash_combine(hash, image_format);
		Engine::Math::hash_combine(hash, usage_flags);
		Engine::Math::hash_combine(hash, sampler_address_mode);
		Engine::Math::hash_combine(hash, filtering);
		Engine::Math::hash_combine(hash, anisotropy);
		
		return hash;
	}
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

class TextureResource : public GPUResource
{
public:
	virtual ~TextureResource()
	{
		destroy();
	}

	virtual void destroy();

	VkBuffer bufferHandle = nullptr;
	VmaAllocation allocation = nullptr;
	VkImage imageHandle = nullptr;
	VkSampler sampler = nullptr;

	struct ImageView
	{
		void cleanup();

		int mip = 0;
		int layer = 0;
		// ImageView needs to gain some information about how to render into this image
		VkImageView image_view = nullptr;
	};
	std::vector<ImageView> image_views;
};

class Texture : public Asset
{
public:
	std::shared_ptr<TextureResource> resource;
private:
	Texture(TextureDescription description);
public:
	virtual ~Texture();

	ASSET_TYPE getAssetType() const override { return ASSET_TYPE_TEXTURE; }

	void cleanup();
	void fill();
	void fill(const void* sourceData);
	void load(const char* path);
	void loadCubemap(const char * pos_x_path, const char * neg_x_path, const char * pos_y_path, const char * neg_y_path, const char * pos_z_path, const char * neg_z_path);

	VkSampler getSampler() { return resource->sampler; }
	std::weak_ptr<TextureResource> getRawResource() { return resource; }

	std::string getPath() const { return path; }
	const TextureDescription &getDescription() const { return description; }
	uint32_t getWidth(int mip = 0) const { return description.width >> mip; }
	uint32_t getHeight(int mip = 0) const { return description.height >> mip; }

	void setDebugName(const char *name);
	std::string getDebugName() const { return debug_name; };

	void transitLayout(CommandBuffer &command_buffer, TextureLayoutType new_layout_type, int mip = -1);
	
	void generateMipmaps(CommandBuffer &command_buffer);

	VkImageView getImageView(int mip = 0, int layer = -1);

	VkFormat getImageFormat() const { return description.image_format; }
	uint32_t getUsageFlags() const { return description.usage_flags; }

	bool isCompressedFormat(VkFormat format) const
	{ 
		return
			format == VK_FORMAT_BC1_RGB_UNORM_BLOCK ||
			format == VK_FORMAT_BC3_UNORM_BLOCK ||
			format == VK_FORMAT_BC5_UNORM_BLOCK ||
			format == VK_FORMAT_BC7_UNORM_BLOCK;
	}

	bool isDepthTexture() const;

	bool isValid() const { return resource->imageHandle != nullptr; }

	static std::shared_ptr<Texture> create(TextureDescription description);
private:
	friend class Swapchain;
	void fill_raw(VkImage image);

	VkImageLayout get_vk_layout(TextureLayoutType layout_type);
	int get_channels_count() const;
	int get_bytes_per_channel() const;
	VkDeviceSize get_image_size() const;

	VkImageUsageFlags get_vk_image_usage_flags() const;

	void copy_buffer_to_image(CommandBuffer &command_buffer, VkBuffer buffer);
	void create_sampler();
private:
	TextureDescription description;

	std::vector<TextureLayoutType> current_layouts; // Image layouts for each mip map
	std::string path = "";
	std::string debug_name = "";
};
