#pragma once
#include "RHI/RHITexture.h"
#include "VulkanUtils.h"
#include "VulkanResources.h"

class VulkanTextureView;
class VulkanTexture final: public RHITexture
{
public:
	VulkanTexture(TextureDescription description): RHITexture(description) {}
	~VulkanTexture();

	void destroy();

	void fill() override;
	void fill(const void *sourceData) override;
	void clear(RHICommandList *cmd_list, const glm::vec4 &color) override;

	void setDebugName(eastl::string name) override;

	void generateMipmaps(RHICommandList *cmd_list) override;

	bool isValid() const override { return image != nullptr; }
	uint32_t getBufferRowPitch() const override { return get_row_size(description.format, getWidth()); }

public:
	VkImage getImage() const { return image->resource; }
	VkImageLayout getNativeLayout() { return get_native_layout(current_layouts[0]); }
	VkFormat getNativeFormat() const { return VulkanUtils::getNativeFormat(description.format); }
	VkImageUsageFlags getNativeUsage() const { return get_vk_image_usage_flags(); }

protected:
	friend class VulkanSwapchain;

	void fill_raw(void *raw_resource) override
	{
		current_layouts.assign(description.mip_levels, TEXTURE_LAYOUT_UNDEFINED);
		if (!image)
			image = std::make_unique<VkImageResource>();
		image->resource = *reinterpret_cast<VkImage *>(raw_resource);
	}

	RHITextureView *create_view(const TextureViewDescription &view_description) override;
	void transit_layout(RHICommandList *cmd_list, TextureLayoutType old_layout_type, TextureLayoutType new_layout_type, uint32_t first_mip, uint32_t mip_count) override;

	void copy_buffer_to_image(VkCommandBuffer command_buffer, VkBuffer buffer)
	{
		eastl::vector<VkBufferImageCopy> regions;
		regions.reserve(description.mip_levels);

		int offset = 0;
		for (int i = 0; i < description.mip_levels; i++)
		{
			VkBufferImageCopy &region = regions.emplace_back();
			region.bufferOffset = offset;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = i;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = description.is_cube ? 6 : 1;
			region.imageOffset = {0, 0, 0};
			region.imageExtent = {
				getWidth(i),
				getHeight(i),
				1
			};
			offset += get_image_size(i);
		}
		
		vkCmdCopyBufferToImage(command_buffer, buffer, image->resource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions.size(), regions.data());
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
			case TEXTURE_LAYOUT_DEPTH_READ:
				return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
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
			case TEXTURE_LAYOUT_UAV:
				return VK_IMAGE_LAYOUT_GENERAL;
				break;
			default:
				ENGINE_ASSERT(false);
		}
	}

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

	VkDeviceSize get_image_size(uint32_t mip) const
	{
		VkDeviceSize image_size = get_slice_size(description.format, getWidth(mip), getHeight(mip));

		if (description.is_cube)
			image_size *= 6;

		return image_size;
	}

	VkDeviceSize get_image_size() const
	{
		VkDeviceSize image_size = 0;
		for (int i = 0; i < description.mip_levels; i++)
			image_size += get_image_size(i);
		return image_size;
	}

	std::unique_ptr<VkImageResource> image = nullptr;
	std::unique_ptr<VkAllocationResource> allocation = nullptr;
};

class VulkanTextureView final: public RHITextureView
{
public:
	VulkanTextureView(TextureViewDescription description);
	~VulkanTextureView();

	VkImageView getImageView() const { return image_view->resource; }

private:
	std::unique_ptr<VkImageViewResource> image_view;
};
