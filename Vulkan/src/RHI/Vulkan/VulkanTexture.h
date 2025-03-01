#pragma once
#include "RHI/RHITexture.h"

class VulkanTexture: public RHITexture, public GPUResource
{
public:
	VulkanTexture(TextureDescription description) : RHITexture(description)
	{
		set_native_format();
	}

	~VulkanTexture();

	void destroy() override;

	void fill() override;
	void fill(const void *sourceData) override;
	void load(const char *path) override;
	void loadCubemap(const char *pos_x_path, const char *neg_x_path, const char *pos_y_path, const char *neg_y_path, const char *pos_z_path, const char *neg_z_path) override;

	void setDebugName(const char *name) override;
	const char *getDebugName() { return debug_name; }

	void transitLayout(RHICommandList *cmd_list, TextureLayoutType new_layout_type, int mip = -1) override;

	VkImageView getImageView(int mip = 0, int layer = -1, bool for_uav = false);
	VkSampler getSampler() { return sampler; }

	bool isValid() const override { return imageHandle != nullptr; }
protected:
	friend class VulkanDynamicRHI;
	friend class VulkanSwapchain;
	void fill_raw(void *raw_resource) override
	{
		current_layouts.resize(description.mip_levels, TEXTURE_LAYOUT_UNDEFINED);
		imageHandle = *reinterpret_cast<VkImage *>(raw_resource);
	}

	void create_sampler();

	void copy_buffer_to_image(VkCommandBuffer command_buffer, VkBuffer buffer)
	{
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = description.is_cube ? 6 : 1;
		region.imageOffset = {0, 0, 0};
		region.imageExtent = {
			description.width,
			description.height,
			1
		};

		vkCmdCopyBufferToImage(command_buffer, buffer, imageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	VkImageLayout get_native_layout(TextureLayoutType layout_type)
	{
		switch (layout_type)
		{
			case TEXTURE_LAYOUT_UNDEFINED:
				return VK_IMAGE_LAYOUT_UNDEFINED;
				break;
			case TEXTURE_LAYOUT_GENERAL:
				return VK_IMAGE_LAYOUT_GENERAL;
				break;
			case TEXTURE_LAYOUT_ATTACHMENT:
				return isDepthTexture() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				break;
			case TEXTURE_LAYOUT_SHADER_READ:
				return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
				break;
			case TEXTURE_LAYOUT_TRANSFER_SRC:
				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				break;
			case TEXTURE_LAYOUT_TRANSFER_DST:
				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				break;
			case TEXTURE_LAYOUT_PRESENT:
				return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				break;
		}
	}

	VkImageLayout get_vk_layout(TextureLayoutType layout_type);
	void set_native_format();
	int get_channels_count() const;
	int get_bytes_per_channel() const;

	VkImageUsageFlags get_vk_image_usage_flags() const
	{
		VkImageUsageFlags flags = 0;
		if ((description.usage_flags & TEXTURE_USAGE_NO_SAMPLED) == 0)
			flags |= VK_IMAGE_USAGE_SAMPLED_BIT;

		if (description.usage_flags & TEXTURE_USAGE_TRANSFER_SRC)
			flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		if (description.usage_flags & TEXTURE_USAGE_TRANSFER_DST)
			flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		if (description.usage_flags & TEXTURE_USAGE_STORAGE)
			flags |= VK_IMAGE_USAGE_STORAGE_BIT;
		if (description.usage_flags & TEXTURE_USAGE_ATTACHMENT)
			flags |= isDepthTexture() ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		return flags;
	}

	VkDeviceSize get_image_size() const
	{
		VkDeviceSize image_size = description.width * description.height * get_bytes_per_channel() * get_channels_count();

		unsigned int block_width = 4;
		unsigned int block_height = 4;
		if (isCompressedFormat())
		{
			unsigned int block_size = 8;
			unsigned int blocks_width = (description.width + block_width - 1) / block_width;
			unsigned int blocks_height = (description.height + block_height - 1) / block_height;

			switch (native_format)
			{
				case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
					block_size = 8;
					break;
				case VK_FORMAT_BC3_UNORM_BLOCK:
				case VK_FORMAT_BC5_UNORM_BLOCK:
				case VK_FORMAT_BC7_UNORM_BLOCK:
					block_size = 16;
					break;
			}
			image_size = blocks_width * blocks_height * block_size;
		}

		if (description.is_cube)
			image_size *= 6;

		return image_size;
	}

	std::vector<TextureLayoutType> current_layouts; // Image layouts for each mip map

	VkFormat native_format = VK_FORMAT_UNDEFINED;

	VkImage imageHandle = nullptr;
	VmaAllocation allocation = nullptr;
	VkSampler sampler = nullptr;

	struct ImageView
	{
		void cleanup();

		int mip = 0;
		int layer = 0;
		bool for_uav = false;
		// ImageView needs to gain some information about how to render into this image
		VkImageView image_view = nullptr;
	};
	std::vector<ImageView> image_views;

	const char *debug_name = "";
};
