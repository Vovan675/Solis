#include "pch.h"
#include "VulkanTexture.h"
#include "VulkanUtils.h"
#include "VulkanDynamicRHI.h"

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

void VulkanTexture::fill()
{
	destroy();
	cleanup();

	image = std::make_unique<VkImageResource>();
	allocation = std::make_unique<VkAllocationResource>();

	VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
	switch (description.sample_count)
	{
		case SAMPLE_COUNT_1: samples = VK_SAMPLE_COUNT_1_BIT; break;
		case SAMPLE_COUNT_2: samples = VK_SAMPLE_COUNT_2_BIT; break;
		case SAMPLE_COUNT_4: samples = VK_SAMPLE_COUNT_4_BIT; break;
		case SAMPLE_COUNT_8: samples = VK_SAMPLE_COUNT_8_BIT; break;
		case SAMPLE_COUNT_16: samples = VK_SAMPLE_COUNT_16_BIT; break;
		case SAMPLE_COUNT_32: samples = VK_SAMPLE_COUNT_32_BIT; break;
		case SAMPLE_COUNT_64: samples = VK_SAMPLE_COUNT_64_BIT; break;
	}

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = is3D() ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = description.width;
	imageInfo.extent.height = description.height;
	imageInfo.extent.depth = description.depth;
	imageInfo.mipLevels = description.mip_levels;
	imageInfo.arrayLayers = description.is_cube ? 6 : description.array_levels;
	imageInfo.flags = description.is_cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	imageInfo.format = getNativeFormat();
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | get_vk_image_usage_flags();
	imageInfo.samples = samples;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	current_layouts.assign(description.mip_levels, TEXTURE_LAYOUT_UNDEFINED);

	VmaAllocationCreateInfo alloc_info{};
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vmaCreateImage(VulkanUtils::getNativeRHI()->allocator, &imageInfo, &alloc_info, &image->resource, &allocation->resource, nullptr);
}

void VulkanTexture::fill(const void *sourceData)
{
	fill();

	// Create staging buffer
	BufferDescription staging_buffer_description;
	staging_buffer_description.size = get_image_size();
	staging_buffer_description.use_staging_buffer = false;
	RHIBufferRef staging_buffer = gDynamicRHI->createBuffer(staging_buffer_description);

	// Copy data to staging
	void* data;
	staging_buffer->map(&data);
	memcpy(data, sourceData, staging_buffer_description.size);
	staging_buffer->unmap();

	VulkanDynamicRHI *rhi = (VulkanDynamicRHI *)gDynamicRHI;
	RHICommandList *upload_cmd_list = gDynamicRHI->getUploadCmdList();
	VulkanCommandList *native_upload_cmd_list = (VulkanCommandList *)upload_cmd_list;
	// Upload buffer data.
	transitLayout(upload_cmd_list, TEXTURE_LAYOUT_TRANSFER_DST);

	copy_buffer_to_image(native_upload_cmd_list->cmd_buffer, ((VulkanBuffer *)staging_buffer.getReference())->getBuffer());

	transitLayout(upload_cmd_list, TEXTURE_LAYOUT_SHADER_READ);
}

void VulkanTexture::clear(RHICommandList *cmd_list, const glm::vec4 &color)
{
	VulkanCommandList *native_cmd_list = (VulkanCommandList *)cmd_list;

	transitLayout(cmd_list, TEXTURE_LAYOUT_TRANSFER_DST);

	VkImageSubresourceRange range{};
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.levelCount = description.mip_levels;
	range.layerCount = description.is_cube ? 6 : description.array_levels;

	if (isDepthTexture())
	{
		range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		VkClearDepthStencilValue clear_value{};
		clear_value.depth = color.x;
		vkCmdClearDepthStencilImage(native_cmd_list->cmd_buffer, image->resource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range);
		return;
	}

	VkClearColorValue clear_value;
	memcpy(clear_value.float32, &color.x, sizeof(clear_value.float32));
	vkCmdClearColorImage(native_cmd_list->cmd_buffer, image->resource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_value, 1, &range);
}

void VulkanTexture::setDebugName(eastl::string name)
{
	debug_name = name;
	VulkanUtils::setDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)image->resource, name.c_str());
}

void VulkanTexture::transit_layout(RHICommandList *cmd_list, TextureLayoutType old_layout_type, TextureLayoutType new_layout_type, uint32_t first_mip, uint32_t mip_count)
{
	VkImageAspectFlags aspect_flags = isDepthTexture() ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageLayout old_layout = get_native_layout(old_layout_type);
	VkImageLayout new_layout = get_native_layout(new_layout_type);
	VkPipelineStageFlags2 src_stage_mask; VkAccessFlags2 src_access_mask;
	VkPipelineStageFlags2 dst_stage_mask; VkAccessFlags2 dst_access_mask;

	switch (old_layout_type)
	{
		case TEXTURE_LAYOUT_UNDEFINED:
			src_stage_mask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
			src_access_mask = VK_ACCESS_2_MEMORY_READ_BIT;
			break;
		case TEXTURE_LAYOUT_GENERAL:
			src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			src_access_mask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
			break;
		case TEXTURE_LAYOUT_ATTACHMENT:
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
		case TEXTURE_LAYOUT_DEPTH_READ:
			assert(isDepthTexture());
			src_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			src_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			break;
		case TEXTURE_LAYOUT_SHADER_READ:
			src_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			src_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
			break;
		case TEXTURE_LAYOUT_TRANSFER_SRC:
			src_stage_mask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
			src_access_mask = VK_ACCESS_2_TRANSFER_READ_BIT;
			break;
		case TEXTURE_LAYOUT_TRANSFER_DST:
			src_stage_mask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
			src_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			break;
		case TEXTURE_LAYOUT_PRESENT:
			src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			src_access_mask = VK_ACCESS_2_NONE;
			break;
		case TEXTURE_LAYOUT_UAV:
			src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
			break;
		default:
			ENGINE_ASSERT(false);
	}

	switch (new_layout_type)
	{
		case TEXTURE_LAYOUT_GENERAL:
			dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
			break;
		case TEXTURE_LAYOUT_ATTACHMENT:
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
		case TEXTURE_LAYOUT_DEPTH_READ:
			assert(isDepthTexture());
			dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dst_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			break;
		case TEXTURE_LAYOUT_SHADER_READ:
			dst_stage_mask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
			break;
		case TEXTURE_LAYOUT_TRANSFER_SRC:
			dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
			dst_access_mask = VK_ACCESS_2_TRANSFER_READ_BIT;
			break;
		case TEXTURE_LAYOUT_TRANSFER_DST:
			dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
			dst_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			break;
		case TEXTURE_LAYOUT_PRESENT:
			dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			dst_access_mask = VK_ACCESS_2_NONE;
			break;
		case TEXTURE_LAYOUT_UAV:
			dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
			break;
		default:
			ENGINE_ASSERT(false);
	}

	int layer_count = description.is_cube ? 6 : description.array_levels;

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
	int faces = description.is_cube ? 6 : 1;
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image->resource;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = faces; // Barrier all faces at once
	barrier.subresourceRange.levelCount = 1; // Barrier for only one mip map

	int32_t mip_width = description.width;
	int32_t mip_height = description.height;

	transitLayout(cmd_list, TEXTURE_LAYOUT_TRANSFER_DST);
	for (uint32_t i = 1; i < description.mip_levels; i++)
	{
		// Barrier to transfer read
		transitLayout(cmd_list, TEXTURE_LAYOUT_TRANSFER_SRC, i - 1);
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
		transitLayout(cmd_list, TEXTURE_LAYOUT_SHADER_READ, i - 1);
	}

	transitLayout(cmd_list, TEXTURE_LAYOUT_SHADER_READ, description.mip_levels - 1);

	/*
	if ((description.usage_flags & TEXTURE_USAGE_ATTACHMENT) == 0)
		return;

	if (description.mip_levels < 2)
		return;
	transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT, 1);
	auto &p = gGlobalPipeline;
	cmd_list->setRenderTargets({this}, nullptr, 0, 1, true);
	p->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/mipmaps.hlsl", FRAGMENT_SHADER));

	gDynamicRHI->setTexture(1, this);

	// Render quad
	cmd_list->drawInstanced(6, 1, 0, 0);

	cmd_list->resetRenderTargets();
	transitLayout(cmd_list, TEXTURE_LAYOUT_SHADER_READ, 1);
	*/
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

	// -1 = all mips
	viewInfo.subresourceRange.baseMipLevel = description.mip == -1 ? 0 : description.mip;
	viewInfo.subresourceRange.levelCount = description.mip == -1 ? native_texture->getMipLevels() : 1;

	// -1 = all layers
	if (native_texture->is3D())
	{
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
		viewInfo.subresourceRange.layerCount = 1;
	} else if (native_texture->getDescription().is_cube)
	{
		if (description.view_type == TextureViewType::SHADER_RESOURCE_STORAGE)
		{
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 6;
		} else
		{
			if (description.layer == -1)
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
	} else if (native_texture->getDescription().array_levels > 1)
	{
		if (description.layer == -1)
		{
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = native_texture->getDescription().array_levels;
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

	if (description.view_type == TextureViewType::SHADER_RESOURCE || description.view_type == TextureViewType::SHADER_RESOURCE_STORAGE)
		bindless_index = gDynamicRHI->getBindlessResources()->addTexture(this);
}

VulkanTextureView::~VulkanTextureView()
{
	gDynamicRHI->releaseGPUResource(image_view.release());
	if (gDynamicRHI && gDynamicRHI->getBindlessResources())
		gDynamicRHI->getBindlessResources()->removeTexture(this);
}
