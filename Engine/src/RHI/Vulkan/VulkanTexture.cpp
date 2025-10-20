#include "pch.h"
#include "VulkanTexture.h"
#include "VulkanUtils.h"
#include "VulkanDynamicRHI.h"
#include "Rendering/GlobalPipeline.h"


VulkanTexture::~VulkanTexture()
{
	destroy();
}

void VulkanTexture::destroy()
{
	auto native_rhi = VulkanUtils::getNativeRHI();

	for (auto &view : image_views)
		native_rhi->releaseGPUResource(view.image_view.release());
	image_views.clear();

	
	native_rhi->releaseGPUResource(image.release());
	native_rhi->releaseGPUResource(allocation.release());

	if (gDynamicRHI && gDynamicRHI->getBindlessResources())
	{
		gDynamicRHI->getBindlessResources()->removeTexture(this);
	}
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
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = description.width;
	imageInfo.extent.height = description.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = description.mip_levels;
	imageInfo.arrayLayers = description.is_cube ? 6 : description.array_levels;
	imageInfo.flags = description.is_cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	imageInfo.format = native_format;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | get_vk_image_usage_flags();
	imageInfo.samples = samples;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	current_layouts.resize(description.mip_levels, TEXTURE_LAYOUT_UNDEFINED);

	VmaAllocationCreateInfo alloc_info{};
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vmaCreateImage(VulkanUtils::getNativeRHI()->allocator, &imageInfo, &alloc_info, &image->resource, &allocation->resource, nullptr);

	getImageView();
	gDynamicRHI->getBindlessResources()->addTexture(this);
}

void VulkanTexture::fill(const void *sourceData)
{
	fill();

	// Create staging buffer
	VkDeviceSize image_size = get_image_size();
	VkBuffer stagingBuffer;
	VmaAllocation stagingAllocation;
	VulkanUtils::createBuffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAllocation);

	// Copy data to staging
	void *data;
	vmaMapMemory(VulkanUtils::getNativeRHI()->allocator, stagingAllocation, &data);
	memcpy(data, sourceData, static_cast<size_t>(image_size));
	vmaUnmapMemory(VulkanUtils::getNativeRHI()->allocator, stagingAllocation);


	{
		VulkanDynamicRHI *rhi = (VulkanDynamicRHI *)gDynamicRHI;
		RHICommandList *copy_cmd_list = gDynamicRHI->getCmdListCopy();
		VulkanCommandList *native_copy_cmd_list = (VulkanCommandList *)copy_cmd_list;
		// Upload buffer data.
		copy_cmd_list->open();

		transitLayout(copy_cmd_list, TEXTURE_LAYOUT_TRANSFER_DST);

		copy_buffer_to_image(native_copy_cmd_list->cmd_buffer, stagingBuffer);

		transitLayout(copy_cmd_list, TEXTURE_LAYOUT_SHADER_READ);

		copy_cmd_list->close();

		gDynamicRHI->getCmdQueueCopy()->execute(copy_cmd_list);
		gDynamicRHI->getCmdQueueCopy()->waitIdle();
	}

	// Destroy unused buffers
	vkDestroyBuffer(VulkanUtils::getNativeRHI()->device->logicalHandle, stagingBuffer, nullptr);
	vmaFreeMemory(VulkanUtils::getNativeRHI()->allocator, stagingAllocation);
}

void VulkanTexture::load(const char *path)
{
	Image image(path);
	asset_handle = image.asset_handle;

	description.width = image.getWidth();
	description.height = image.getHeight();
	description.mip_levels = image.getMipLevels();
	description.format = image.getFormat();
	set_native_format();
	fill(image.getRawData().data());

	this->path = path;
}

void VulkanTexture::loadEquirectangularCubemap(const char *path)
{
	TextureDescription desc{};
	desc.usage_flags = TEXTURE_USAGE_ATTACHMENT | TEXTURE_USAGE_TRANSFER_SRC | TEXTURE_USAGE_TRANSFER_DST;
	RHITextureRef equirect_texture = gDynamicRHI->createTexture(desc);
	equirect_texture->load(path);

	description.is_cube = true;
	description.format = FORMAT_R32G32B32A32_SFLOAT;
	set_native_format();
	description.width = equirect_texture->getHeight();
	description.height = equirect_texture->getHeight();
	description.mip_levels = 1;

	fill();
	RHICommandList *cmd_list = gDynamicRHI->getCmdList();

	transitLayout(cmd_list, TEXTURE_LAYOUT_GENERAL);

	RHITextureRef texture = nullptr;

	auto &p = gGlobalPipeline;

	p->reset();
	p->setIsComputePipeline(true);
	p->setComputeShader(gDynamicRHI->createShader(L"shaders/equirect_to_cubemap.hlsl", COMPUTE_SHADER));
	p->flush();
	p->bind(cmd_list);

	gDynamicRHI->setUAVTexture(0, this);
	struct Uniforms
	{
		uint32_t equirect_tex_id;
	} uniforms;
	uniforms.equirect_tex_id = gDynamicRHI->getBindlessResources()->getTextureIndex(equirect_texture);
	gDynamicRHI->setConstantBufferData(1, &uniforms, sizeof(uniforms));

	cmd_list->dispatch(getWidth() / 32, getHeight() / 32, 6);
	p->unbind(cmd_list);

	transitLayout(cmd_list, TEXTURE_LAYOUT_SHADER_READ);

	this->path = path;
}

void VulkanTexture::setDebugName(const char *name)
{
	debug_name = name;
	VulkanUtils::setDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)image->resource, name);
}

void VulkanTexture::transitLayout(RHICommandList *cmd_list, TextureLayoutType new_layout_type, int mip)
{
	bool barriers_required = false;

	size_t difference_mip_index; // Index where mip layouts start differ
	size_t difference_mip_count; // How many mip layouts differ

	size_t start_index = mip == -1 ? 0 : mip;
	size_t end_index = mip == -1 ? description.mip_levels : start_index + 1;
	for (size_t i = start_index; i < end_index; i++)
	{
		if (current_layouts[i] != new_layout_type)
		{
			barriers_required = true;
			difference_mip_index = i;
			difference_mip_count = end_index - i;
			break;
		}
	}

	if (!barriers_required) return;

	VkImageAspectFlags aspect_flags = isDepthTexture() ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	TextureLayoutType current_layout = current_layouts[difference_mip_index];
	VkImageLayout old_layout = get_native_layout(current_layout);
	VkImageLayout new_layout = get_native_layout(new_layout_type);
	VkPipelineStageFlags2 src_stage_mask; VkAccessFlags2 src_access_mask;
	VkPipelineStageFlags2 dst_stage_mask; VkAccessFlags2 dst_access_mask;

	switch (current_layout)
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
				src_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
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
				dst_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			} else
			{
				dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
				dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
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
	}

	int layer_count = description.is_cube ? 6 : description.array_levels;

	VulkanCommandList *native_cmd_list = (VulkanCommandList *)cmd_list;
	VulkanUtils::cmdImageMemoryBarrier(native_cmd_list->cmd_buffer,
									   src_stage_mask, src_access_mask,
									   dst_stage_mask, dst_access_mask,
									   old_layout, new_layout,
									   image->resource, aspect_flags,
									   difference_mip_count, layer_count,
									   difference_mip_index, 0);

	for (size_t i = difference_mip_index; i < difference_mip_index + difference_mip_count; i++)
	{
		current_layouts[i] = new_layout_type;
	}
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

	p->unbind(cmd_list);
	cmd_list->resetRenderTargets();
	transitLayout(cmd_list, TEXTURE_LAYOUT_SHADER_READ, 1);
	*/
}

VkImageView VulkanTexture::getImageView(int mip, int layer, bool for_uav)
{
	// Find already created
	for (const auto &view : image_views)
	{
		if (view.mip == mip && view.layer == layer && view.for_uav == for_uav)
			return view.image_view->resource;
	}

	// Otherwise create new
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image->resource;
	viewInfo.format = native_format;
	viewInfo.subresourceRange.aspectMask = isDepthTexture() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	// -1 = all mips
	viewInfo.subresourceRange.baseMipLevel = mip == -1 ? 0 : mip;
	viewInfo.subresourceRange.levelCount = mip == -1 ? description.mip_levels : 1;

	// -1 = all layers
	if (description.is_cube)
	{
		if (for_uav)
		{
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 6;
		} else
		{
			if (layer == -1)
			{
				viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
				viewInfo.subresourceRange.baseArrayLayer = 0;
				viewInfo.subresourceRange.layerCount = 6;
			} else
			{
				viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewInfo.subresourceRange.baseArrayLayer = layer;
				viewInfo.subresourceRange.layerCount = 1;
			}
		}
	} else if (description.array_levels > 1)
	{
		if (layer == -1)
		{
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = description.array_levels;
		} else
		{
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.subresourceRange.baseArrayLayer = layer;
			viewInfo.subresourceRange.layerCount = 1;
		}

	} else
	{
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.subresourceRange.layerCount = 1;
	}

	VkImageView image_view_resource;
	CHECK_ERROR(vkCreateImageView(VulkanUtils::getNativeRHI()->device->logicalHandle, &viewInfo, nullptr, &image_view_resource));

	if (debug_name)
		VulkanUtils::setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)image_view_resource, debug_name);

	ImageView &view = image_views.emplace_back();
	view.mip = mip;
	view.layer = layer;
	view.image_view = std::make_unique<VkImageViewResource>();
	view.image_view->resource = image_view_resource;
	
	return image_view_resource;
}

VkImageLayout VulkanTexture::get_vk_layout(TextureLayoutType layout_type)
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

void VulkanTexture::set_native_format()
{
	native_format = VulkanUtils::getNativeFormat(description.format);
}
