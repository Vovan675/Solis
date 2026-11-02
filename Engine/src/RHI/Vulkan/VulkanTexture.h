#pragma once
#include "RHI/RHITexture.h"
#include "VulkanUtils.h"
#include "VulkanResources.h"

class VulkanTextureView;
class VulkanTexture final: public RHITexture
{
public:
	VulkanTexture(const TextureDescription &description, const void *initial_data);
	VulkanTexture(const TextureDescription &description, VkImage swapchain_image);
	~VulkanTexture();

	void destroy();

	void setDebugName(const char *name) override;

	void generateMipmaps(RHICommandList *cmd_list) override;

	bool isValid() const override { return image != nullptr; }
	uint32_t getBufferRowPitch() const override { return getFormatRowSize(description.format, getWidth()); }

public:
	VkImage getImage() const { return image->resource; }
	VkImageLayout getNativeLayout() { return get_native_layout(current_states[0]); }
	VkFormat getNativeFormat() const { return VulkanUtils::getNativeFormat(description.format); }
	VkImageUsageFlags getNativeUsage() const { return get_vk_image_usage_flags(); }

protected:
	friend class VulkanSwapchain;

	void recreate(const TextureDescription &new_description, const void *initial_data) override;
	void create_resource(const void *initial_data);
	RHITextureView *create_view(const TextureViewDescription &view_description) override;
	void transit_state(RHICommandList *cmd_list, ResourceState old_state, ResourceState new_state, uint32_t first_mip, uint32_t mip_count) override;

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
			region.imageSubresource.layerCount = isCube() ? 6 : 1;
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

	VkImageLayout get_native_layout(ResourceState state)
	{
		switch (state)
		{
			case ResourceState::UNDEFINED:
				return VK_IMAGE_LAYOUT_UNDEFINED;
				break;
			case ResourceState::COMMON:
				return VK_IMAGE_LAYOUT_GENERAL;
				break;
			case ResourceState::RENDER_TARGET:
				return isDepthTexture() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				break;
			case ResourceState::DEPTH_READ:
				return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				break;
			case ResourceState::SHADER_RESOURCE:
				return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
				break;
			case ResourceState::COPY_SRC:
				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				break;
			case ResourceState::COPY_DST:
				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				break;
			case ResourceState::PRESENT:
				return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				break;
			case ResourceState::UNORDERED_ACCESS:
				return VK_IMAGE_LAYOUT_GENERAL;
				break;
			default:
				ENGINE_ASSERT(false);
		}
	}

	VkImageUsageFlags get_vk_image_usage_flags() const
	{
		VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (!hasAnyFlags(description.usage_flags, TextureUsage::NO_SAMPLED))
			flags |= VK_IMAGE_USAGE_SAMPLED_BIT;

		if (hasAnyFlags(description.usage_flags, TextureUsage::UNORDERED_ACCESS))
			flags |= VK_IMAGE_USAGE_STORAGE_BIT;
		if (hasAnyFlags(description.usage_flags, TextureUsage::RENDER_TARGET))
			flags |= isDepthTexture() ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		return flags;
	}

	VkDeviceSize get_image_size(uint32_t mip) const
	{
		VkDeviceSize image_size = getFormatSliceSize(description.format, getWidth(mip), getHeight(mip));

		if (isCube())
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
