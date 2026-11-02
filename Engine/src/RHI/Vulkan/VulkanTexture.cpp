#include "pch.h"
#include "VulkanTexture.h"
#include "VulkanUtils.h"
#include "VulkanDynamicRHI.h"

VulkanTexture::VulkanTexture(const TextureDescription &description, const void *initial_data): RHITexture(description)
{
	create_resource(initial_data);
}

VulkanTexture::VulkanTexture(const TextureDescription &description, VkImage swapchain_image): RHITexture(description)
{
	current_states.assign(description.mip_levels, ResourceState::UNDEFINED);
	image = std::make_unique<VkImageResource>();
	image->resource = swapchain_image;
}

VulkanTexture::~VulkanTexture()
{
	destroy();
}

void VulkanTexture::destroy()
{
	auto native_rhi = VulkanUtils::getNativeRHI();

	views.clear();

	native_rhi->releaseGPUResource(image.release());
	native_rhi->releaseGPUResource(allocation.release());
}

void VulkanTexture::recreate(const TextureDescription &new_description, const void *initial_data)
{
	destroy();
	description = new_description;
	create_resource(initial_data);
}

void VulkanTexture::create_resource(const void *initial_data)
{
	image = std::make_unique<VkImageResource>();
	allocation = std::make_unique<VkAllocationResource>();

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = is3D() ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = description.width;
	imageInfo.extent.height = description.height;
	imageInfo.extent.depth = description.depth;
	imageInfo.mipLevels = description.mip_levels;
	imageInfo.arrayLayers = isCube() ? 6 : description.array_layers;
	imageInfo.flags = isCube() ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	imageInfo.format = getNativeFormat();
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = get_vk_image_usage_flags();
	imageInfo.samples = (VkSampleCountFlagBits)description.sample_count;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	current_states.assign(description.mip_levels, ResourceState::UNDEFINED);

	VmaAllocationCreateInfo alloc_info{};
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vmaCreateImage(VulkanUtils::getNativeRHI()->allocator, &imageInfo, &alloc_info, &image->resource, &allocation->resource, nullptr);

	if (!initial_data)
		return;

	// Create staging buffer
	BufferDescription staging_buffer_description;
	staging_buffer_description.size = get_image_size();
	staging_buffer_description.memory = BufferMemory::UPLOAD;
	RHIBufferRef staging_buffer = gDynamicRHI->createBuffer(staging_buffer_description, initial_data);

	RHICommandList *upload_cmd_list = gDynamicRHI->getUploadCmdList();
	VulkanCommandList *native_upload_cmd_list = (VulkanCommandList *)upload_cmd_list;
	// Upload buffer data.
	transitState(upload_cmd_list, ResourceState::COPY_DST);

	copy_buffer_to_image(native_upload_cmd_list->cmd_buffer, ((VulkanBuffer *)staging_buffer.getReference())->getBuffer());

	transitState(upload_cmd_list, ResourceState::SHADER_RESOURCE);
}

void VulkanTexture::setDebugName(const char *name)
{
	debug_name = name;
	VulkanUtils::setDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)image->resource, name);
}

void VulkanTexture::transit_state(RHICommandList *cmd_list, ResourceState old_state, ResourceState new_state, uint32_t first_mip, uint32_t mip_count)
{
	VkImageAspectFlags aspect_flags = isDepthTexture() ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageLayout old_layout = get_native_layout(old_state);
	VkImageLayout new_layout = get_native_layout(new_state);
	VkPipelineStageFlags2 src_stage_mask; VkAccessFlags2 src_access_mask;
	VkPipelineStageFlags2 dst_stage_mask; VkAccessFlags2 dst_access_mask;

	switch (old_state)
	{
		case ResourceState::UNDEFINED:
			src_stage_mask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
			src_access_mask = VK_ACCESS_2_MEMORY_READ_BIT;
			break;
		case ResourceState::COMMON:
			src_stage_mask = VulkanUtils::getAllShaderStages();
			src_access_mask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
			break;
		case ResourceState::RENDER_TARGET:
			if (isDepthTexture())
			{
				src_stage_mask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
				src_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			} else
			{
				src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
				src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			}
			break;
		case ResourceState::DEPTH_READ:
			assert(isDepthTexture());
			src_stage_mask = VulkanUtils::getAllShaderStages() | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
			src_access_mask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			break;
		case ResourceState::SHADER_RESOURCE:
			src_stage_mask = VulkanUtils::getAllShaderStages();
			src_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
			break;
		case ResourceState::COPY_SRC:
			src_stage_mask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
			src_access_mask = VK_ACCESS_2_TRANSFER_READ_BIT;
			break;
		case ResourceState::COPY_DST:
			src_stage_mask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
			src_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			break;
		case ResourceState::PRESENT:
			src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			src_access_mask = VK_ACCESS_2_NONE;
			break;
		case ResourceState::UNORDERED_ACCESS:
			src_stage_mask = VulkanUtils::getAllShaderStages();
			src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
			break;
		default:
			ENGINE_ASSERT(false);
	}

	switch (new_state)
	{
		case ResourceState::COMMON:
			dst_stage_mask = VulkanUtils::getAllShaderStages();
			dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
			break;
		case ResourceState::RENDER_TARGET:
			if (isDepthTexture())
			{
				dst_stage_mask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
				dst_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			} else
			{
				dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
				dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			}
			break;
		case ResourceState::DEPTH_READ:
			assert(isDepthTexture());
			dst_stage_mask = VulkanUtils::getAllShaderStages() | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
			dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			break;
		case ResourceState::SHADER_RESOURCE:
			dst_stage_mask = VulkanUtils::getAllShaderStages();
			dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
			break;
		case ResourceState::COPY_SRC:
			dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
			dst_access_mask = VK_ACCESS_2_TRANSFER_READ_BIT;
			break;
		case ResourceState::COPY_DST:
			dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
			dst_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			break;
		case ResourceState::PRESENT:
			dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			dst_access_mask = VK_ACCESS_2_NONE;
			break;
		case ResourceState::UNORDERED_ACCESS:
			dst_stage_mask = VulkanUtils::getAllShaderStages();
			dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
			break;
		default:
			ENGINE_ASSERT(false);
	}

	int layer_count = isCube() ? 6 : description.array_layers;

	VulkanCommandList *native_cmd_list = (VulkanCommandList *)cmd_list;
	VulkanUtils::cmdImageMemoryBarrier(native_cmd_list->cmd_buffer,
									   src_stage_mask, src_access_mask,
									   dst_stage_mask, dst_access_mask,
									   old_layout, new_layout,
									   image->resource, aspect_flags,
									   mip_count, layer_count,
									   first_mip, 0);
}

void VulkanTexture::generateMipmaps(RHICommandList *cmd_list)
{
	if (!isRenderTargetTexture())
	{
		CORE_ERROR("Generating mipmaps at runtime for non render target texture is not supported");
		return;
	}

	PROFILE_GPU_FUNCTION(cmd_list);
	int faces = isCube() ? 6 : 1;
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image->resource;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = faces; // Barrier all faces at once
	barrier.subresourceRange.levelCount = 1; // Barrier for only one mip map

	int32_t mip_width = description.width;
	int32_t mip_height = description.height;

	transitState(cmd_list, ResourceState::COPY_DST);
	for (uint32_t i = 1; i < description.mip_levels; i++)
	{
		// Barrier to transfer read
		transitState(cmd_list, ResourceState::COPY_SRC, i - 1);
		VkImageBlit blit{};

		// Blit src
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mip_width, mip_height, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = faces; // All cubemap faces at once

		// Blit dst
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0; 
		blit.dstSubresource.layerCount = faces; // All cubemap faces at once

		vkCmdBlitImage(((VulkanCommandList *)cmd_list)->cmd_buffer,
					   image->resource, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					   image->resource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					   1, &blit,
					   VK_FILTER_LINEAR);

		if (mip_width > 1) mip_width /= 2;
		if (mip_height > 1) mip_height /= 2;

		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		// Barrier to shader read
		transitState(cmd_list, ResourceState::SHADER_RESOURCE, i - 1);
	}

	transitState(cmd_list, ResourceState::SHADER_RESOURCE, description.mip_levels - 1);
}

RHITextureView *VulkanTexture::create_view(const TextureViewDescription &view_description)
{
	return new VulkanTextureView(view_description);
}

VulkanTextureView::VulkanTextureView(TextureViewDescription description): RHITextureView(description)
{
	VulkanTexture *native_texture = (VulkanTexture *)description.texture;
	VkFormat native_format = VulkanUtils::getNativeFormat(native_texture->getFormat());

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = native_texture->getImage();
	viewInfo.format = native_format;
	viewInfo.subresourceRange.aspectMask = native_texture->isDepthTexture() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	viewInfo.subresourceRange.baseMipLevel = description.mip == ALL_MIPS ? 0 : description.mip;
	viewInfo.subresourceRange.levelCount = description.mip == ALL_MIPS ? native_texture->getMipLevels() : 1;

	if (native_texture->is3D())
	{
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
		viewInfo.subresourceRange.layerCount = 1;
	} else if (native_texture->isCube())
	{
		if (description.view_type == TextureViewType::UNORDERED_ACCESS)
		{
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 6;
		} else
		{
			if (description.layer == ALL_LAYERS)
			{
				viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
				viewInfo.subresourceRange.baseArrayLayer = 0;
				viewInfo.subresourceRange.layerCount = 6;
			} else
			{
				viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewInfo.subresourceRange.baseArrayLayer = description.layer;
				viewInfo.subresourceRange.layerCount = 1;
			}
		}
	} else if (native_texture->getDescription().array_layers > 1)
	{
		if (description.layer == ALL_LAYERS)
		{
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = native_texture->getDescription().array_layers;
		} else
		{
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.subresourceRange.baseArrayLayer = description.layer;
			viewInfo.subresourceRange.layerCount = 1;
		}

	} else
	{
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.subresourceRange.layerCount = 1;
	}

	VkImageView image_view_resource;
	CHECK_ERROR(vkCreateImageView(VulkanUtils::getNativeRHI()->device->logicalHandle, &viewInfo, nullptr, &image_view_resource));

	if (native_texture->getDebugName())
		VulkanUtils::setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)image_view_resource, native_texture->getDebugName());

	image_view = std::make_unique<VkImageViewResource>();
	image_view->resource = image_view_resource;

	if (description.view_type == TextureViewType::SHADER_RESOURCE || description.view_type == TextureViewType::UNORDERED_ACCESS)
		bindless_index = gDynamicRHI->getBindlessResources()->addTexture(this);
}

VulkanTextureView::~VulkanTextureView()
{
	gDynamicRHI->releaseGPUResource(image_view.release());
	if (gDynamicRHI && gDynamicRHI->getBindlessResources())
		gDynamicRHI->getBindlessResources()->removeTexture(this);
}
