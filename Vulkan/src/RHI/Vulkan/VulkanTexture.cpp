#include "pch.h"
#include "VulkanTexture.h"
#include "stb_image.h"
#include "tinyddsloader.h"
#include "VulkanUtils.h"
#include "VulkanDynamicRHI.h"


VulkanTexture::~VulkanTexture()
{
	destroy();
}

void VulkanTexture::destroy()
{
	auto native_rhi = VulkanUtils::getNativeRHI();
	if (sampler)
	{
		native_rhi->gpu_release_queue[native_rhi->current_frame].push_back({RESOURCE_TYPE_SAMPLER, sampler});
		sampler = nullptr;
	}

	for (auto &view : image_views)
	{
		if (view.image_view)
			native_rhi->gpu_release_queue[native_rhi->current_frame].push_back({RESOURCE_TYPE_IMAGE_VIEW, view.image_view});
	}
	image_views.clear();

	if (imageHandle)
	{
		native_rhi->gpu_release_queue[native_rhi->current_frame].push_back({RESOURCE_TYPE_IMAGE, imageHandle});
		imageHandle = nullptr;
	}

	if (allocation)
	{
		native_rhi->gpu_release_queue[native_rhi->current_frame].push_back({RESOURCE_TYPE_MEMORY, allocation});
		allocation = nullptr;
	}

	if (gDynamicRHI && gDynamicRHI->getBindlessResources())
	{
		gDynamicRHI->getBindlessResources()->removeTexture(this);
	}
}

void VulkanTexture::fill()
{
	destroy();
	cleanup();

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
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | get_vk_image_usage_flags();
	imageInfo.samples = samples;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	current_layouts.resize(description.mip_levels, TEXTURE_LAYOUT_UNDEFINED);

	VmaAllocationCreateInfo alloc_info{};
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vmaCreateImage(VulkanUtils::getNativeRHI()->allocator, &imageInfo, &alloc_info, &imageHandle, &allocation, nullptr);

	getImageView();
	create_sampler();
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
		// Create mipmaps
		////generateMipmaps(command_buffer);

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
	int texWidth, texHeight, texChannels;

	std::filesystem::path tex_path(path);
	std::string ext = tex_path.extension().string();
	tinyddsloader::DDSFile file;
	void *pixels;
	if (tex_path.extension() == ".dds")
	{
		tinyddsloader::Result result = file.Load(path);
		if (result != tinyddsloader::Success)
		{
			CORE_ERROR("Loading texture error");
			return;
		}

		file.Flip();
		texWidth = file.GetWidth();
		texHeight = file.GetHeight();
		auto *data = file.GetImageData();
		pixels = data->m_mem;

		auto format = file.GetFormat();
		if (format == tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm)
			native_format = VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		else if (format == tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm)
			native_format = VK_FORMAT_BC3_UNORM_BLOCK;
		else if (format == tinyddsloader::DDSFile::DXGIFormat::BC5_UNorm)
			native_format = VK_FORMAT_BC5_UNORM_BLOCK;
		else if (format == tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm)
			native_format = VK_FORMAT_BC7_UNORM_BLOCK;
		else
			CORE_ERROR("Invalid texture format");

		description.width = texWidth;
		description.height = texHeight;
		description.mip_levels = 1;
		fill(pixels);
	} else
	{
		stbi_set_flip_vertically_on_load(1);
		pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

		if (!pixels)
		{
			CORE_ERROR("Loading texture error");
			return;
		}

		description.width = texWidth;
		description.height = texHeight;
		description.mip_levels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
		fill(pixels);

		stbi_image_free(pixels);
	}
	this->path = path;
}

void VulkanTexture::loadCubemap(const char *pos_x_path, const char *neg_x_path, const char *pos_y_path, const char *neg_y_path, const char *pos_z_path, const char *neg_z_path)
{
	int texWidth, texHeight, texChannels;
	stbi_set_flip_vertically_on_load(0);
	stbi_uc *pos_x = stbi_load(pos_x_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pos_x)
		CORE_ERROR("Loading texture error");

	stbi_uc *neg_x = stbi_load(neg_x_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!neg_x)
		CORE_ERROR("Loading texture error");

	stbi_uc *pos_y = stbi_load(pos_y_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pos_y)
		CORE_ERROR("Loading texture error");

	stbi_uc *neg_y = stbi_load(neg_y_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!neg_y)
		CORE_ERROR("Loading texture error");

	stbi_uc *pos_z = stbi_load(pos_z_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pos_z)
		CORE_ERROR("Loading texture error");

	stbi_uc *neg_z = stbi_load(neg_z_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!neg_z)
		CORE_ERROR("Loading texture error");

	description.is_cube = true;
	description.format = FORMAT_R8G8B8A8_UNORM;
	set_native_format();
	description.width = texWidth;
	description.height = texHeight;
	description.mip_levels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;

	texChannels = 4;
	size_t face_size = texWidth * texHeight * texChannels;
	stbi_uc *data = new stbi_uc[face_size * 6];
	memcpy(data + face_size * 0, pos_x, face_size);
	memcpy(data + face_size * 1, neg_x, face_size);

	memcpy(data + face_size * 2, pos_y, face_size);
	memcpy(data + face_size * 3, neg_y, face_size);

	memcpy(data + face_size * 4, pos_z, face_size);
	memcpy(data + face_size * 5, neg_z, face_size);

	fill(data);

	stbi_image_free(pos_x);
	stbi_image_free(neg_x);

	stbi_image_free(pos_y);
	stbi_image_free(neg_y);

	stbi_image_free(pos_z);
	stbi_image_free(neg_z);

	this->path = pos_x_path;
}

void VulkanTexture::setDebugName(const char *name)
{
	debug_name = name;
	VulkanUtils::setDebugName(VK_OBJECT_TYPE_IMAGE, (uint64_t)imageHandle, name);
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
									   imageHandle, aspect_flags,
									   difference_mip_count, layer_count,
									   difference_mip_index, 0);

	for (size_t i = difference_mip_index; i < difference_mip_index + difference_mip_count; i++)
	{
		current_layouts[i] = new_layout_type;
	}
}

VkImageView VulkanTexture::getImageView(int mip, int layer, bool for_uav)
{
	// Find already created
	for (const auto &view : image_views)
	{
		if (view.mip == mip && view.layer == layer && view.for_uav == for_uav)
			return view.image_view;
	}

	// Otherwise create new
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = imageHandle;
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

	VkImageView image_view;
	CHECK_ERROR(vkCreateImageView(VulkanUtils::getNativeRHI()->device->logicalHandle, &viewInfo, nullptr, &image_view));

	ImageView view;
	view.mip = mip;
	view.layer = layer;
	view.image_view = image_view;

	image_views.push_back(view);
	return image_view;
}

void VulkanTexture::create_sampler()
{
	VkFilter filter = VK_FILTER_LINEAR;
	if (description.filtering == FILTER_LINEAR)
		filter = VK_FILTER_LINEAR;
	else if (description.filtering == FILTER_NEAREST)
		filter = VK_FILTER_NEAREST;

	VkSamplerAddressMode sampler_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	if (description.sampler_mode == SAMPLER_MODE_REPEAT)
		sampler_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	else if (description.sampler_mode == SAMPLER_MODE_CLAMP_TO_EDGE)
		sampler_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	else if (description.sampler_mode == SAMPLER_MODE_CLAMP_TO_BORDER)
		sampler_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = filter;
	samplerInfo.minFilter = filter;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.addressModeU = sampler_mode;
	samplerInfo.addressModeV = sampler_mode;
	samplerInfo.addressModeW = sampler_mode;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	if (description.use_comparison_less)
	{
		samplerInfo.compareEnable = true;
		samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // For shadows hardware comparison
	}

	samplerInfo.anisotropyEnable = description.anisotropy;
	samplerInfo.maxAnisotropy = description.anisotropy ? VulkanUtils::getNativeRHI()->device->physicalProperties.properties.limits.maxSamplerAnisotropy : 1.0f;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = description.mip_levels;


	CHECK_ERROR(vkCreateSampler(VulkanUtils::getNativeRHI()->device->logicalHandle, &samplerInfo, nullptr, &sampler));
}

/*
void Texture::generateMipmaps(RHICommandList *cmd_list) {
	int faces = description.is_cube ? 6 : 1;
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = resource->imageHandle;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = faces; // Barrier all faces at once
	barrier.subresourceRange.levelCount = 1; // Barrier for only one mip map

	int32_t mipWidth = description.width;
	int32_t mipHeight = description.height;

	///transitLayout(command_buffer, TEXTURE_LAYOUT_TRANSFER_DST);
	for (uint32_t i = 1; i < description.mip_levels; i++)
	{
		// Barrier to transfer read
		///transitLayout(command_buffer, TEXTURE_LAYOUT_TRANSFER_SRC, i - 1);
		VkImageBlit blit{};

		// Blit src
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = faces; // All cubemap faces at once

		// Blit dst
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0; 
		blit.dstSubresource.layerCount = faces; // All cubemap faces at once

		vkCmdBlitImage(((VulkanCommandList *)cmd_list)->cmd_buffer,
					   resource->imageHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					   resource->imageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					   1, &blit,
					   VK_FILTER_LINEAR);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;

		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		// Barrier to shader read
		///transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ, i - 1);
	}

	///transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ, description.mip_levels - 1);
}
*/

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

int VulkanTexture::get_channels_count() const
{
	switch (native_format)
	{
		case VK_FORMAT_R8_UNORM:
		case VK_FORMAT_R8_SNORM:
		case VK_FORMAT_R8_USCALED:
		case VK_FORMAT_R8_SSCALED:
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8_SRGB:

		case VK_FORMAT_R16_UNORM:
		case VK_FORMAT_R16_SNORM:
		case VK_FORMAT_R16_USCALED:
		case VK_FORMAT_R16_SSCALED:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16_SFLOAT:

		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_SFLOAT:

		case VK_FORMAT_R64_UINT:
		case VK_FORMAT_R64_SINT:
		case VK_FORMAT_R64_SFLOAT:
			return 1;

		case VK_FORMAT_R8G8_UNORM:
		case VK_FORMAT_R8G8_SNORM:
		case VK_FORMAT_R8G8_USCALED:
		case VK_FORMAT_R8G8_SSCALED:
		case VK_FORMAT_R8G8_UINT:
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R8G8_SRGB:

		case VK_FORMAT_R16G16_UNORM:
		case VK_FORMAT_R16G16_SNORM:
		case VK_FORMAT_R16G16_USCALED:
		case VK_FORMAT_R16G16_SSCALED:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16_SFLOAT:

		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_SFLOAT:

		case VK_FORMAT_R64G64_UINT:
		case VK_FORMAT_R64G64_SINT:
		case VK_FORMAT_R64G64_SFLOAT:
			return 2;

		case VK_FORMAT_R8G8B8_UNORM:
		case VK_FORMAT_R8G8B8_SNORM:
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8_SSCALED:
		case VK_FORMAT_R8G8B8_UINT:
		case VK_FORMAT_R8G8B8_SINT:
		case VK_FORMAT_R8G8B8_SRGB:

		case VK_FORMAT_B8G8R8_UNORM:
		case VK_FORMAT_B8G8R8_SNORM:
		case VK_FORMAT_B8G8R8_USCALED:
		case VK_FORMAT_B8G8R8_SSCALED:
		case VK_FORMAT_B8G8R8_UINT:
		case VK_FORMAT_B8G8R8_SINT:
		case VK_FORMAT_B8G8R8_SRGB:

		case VK_FORMAT_R16G16B16_UNORM:
		case VK_FORMAT_R16G16B16_SNORM:
		case VK_FORMAT_R16G16B16_USCALED:
		case VK_FORMAT_R16G16B16_SSCALED:
		case VK_FORMAT_R16G16B16_UINT:
		case VK_FORMAT_R16G16B16_SINT:
		case VK_FORMAT_R16G16B16_SFLOAT:

		case VK_FORMAT_R32G32B32_UINT:
		case VK_FORMAT_R32G32B32_SINT:
		case VK_FORMAT_R32G32B32_SFLOAT:

		case VK_FORMAT_R64G64B64_UINT:
		case VK_FORMAT_R64G64B64_SINT:
		case VK_FORMAT_R64G64B64_SFLOAT:
			return 3;

		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SNORM:
		case VK_FORMAT_R8G8B8A8_USCALED:
		case VK_FORMAT_R8G8B8A8_SSCALED:
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R8G8B8A8_SRGB:

		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_USCALED:
		case VK_FORMAT_B8G8R8A8_SSCALED:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:

		case VK_FORMAT_R16G16B16A16_UNORM:
		case VK_FORMAT_R16G16B16A16_SNORM:
		case VK_FORMAT_R16G16B16A16_USCALED:
		case VK_FORMAT_R16G16B16A16_SSCALED:
		case VK_FORMAT_R16G16B16A16_UINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:

		case VK_FORMAT_R32G32B32A32_UINT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:

		case VK_FORMAT_R64G64B64A64_UINT:
		case VK_FORMAT_R64G64B64A64_SINT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return 4;
	}
	return 0;
}

int VulkanTexture::get_bytes_per_channel() const
{
	switch (native_format)
	{
		case VK_FORMAT_R8_UNORM:
		case VK_FORMAT_R8_SNORM:
		case VK_FORMAT_R8_USCALED:
		case VK_FORMAT_R8_SSCALED:
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8_SRGB:

		case VK_FORMAT_R8G8_UNORM:
		case VK_FORMAT_R8G8_SNORM:
		case VK_FORMAT_R8G8_USCALED:
		case VK_FORMAT_R8G8_SSCALED:
		case VK_FORMAT_R8G8_UINT:
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R8G8_SRGB:

		case VK_FORMAT_R8G8B8_UNORM:
		case VK_FORMAT_R8G8B8_SNORM:
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8_SSCALED:
		case VK_FORMAT_R8G8B8_UINT:
		case VK_FORMAT_R8G8B8_SINT:
		case VK_FORMAT_R8G8B8_SRGB:

		case VK_FORMAT_B8G8R8_UNORM:
		case VK_FORMAT_B8G8R8_SNORM:
		case VK_FORMAT_B8G8R8_USCALED:
		case VK_FORMAT_B8G8R8_SSCALED:
		case VK_FORMAT_B8G8R8_UINT:
		case VK_FORMAT_B8G8R8_SINT:
		case VK_FORMAT_B8G8R8_SRGB:

		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SNORM:
		case VK_FORMAT_R8G8B8A8_USCALED:
		case VK_FORMAT_R8G8B8A8_SSCALED:
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R8G8B8A8_SRGB:

		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_USCALED:
		case VK_FORMAT_B8G8R8A8_SSCALED:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
			return 1;

		case VK_FORMAT_R16_UNORM:
		case VK_FORMAT_R16_SNORM:
		case VK_FORMAT_R16_USCALED:
		case VK_FORMAT_R16_SSCALED:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16_SFLOAT:

		case VK_FORMAT_R16G16_UNORM:
		case VK_FORMAT_R16G16_SNORM:
		case VK_FORMAT_R16G16_USCALED:
		case VK_FORMAT_R16G16_SSCALED:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16_SFLOAT:

		case VK_FORMAT_R16G16B16_UNORM:
		case VK_FORMAT_R16G16B16_SNORM:
		case VK_FORMAT_R16G16B16_USCALED:
		case VK_FORMAT_R16G16B16_SSCALED:
		case VK_FORMAT_R16G16B16_UINT:
		case VK_FORMAT_R16G16B16_SINT:
		case VK_FORMAT_R16G16B16_SFLOAT:

		case VK_FORMAT_R16G16B16A16_UNORM:
		case VK_FORMAT_R16G16B16A16_SNORM:
		case VK_FORMAT_R16G16B16A16_USCALED:
		case VK_FORMAT_R16G16B16A16_SSCALED:
		case VK_FORMAT_R16G16B16A16_UINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			return 2;

		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_SFLOAT:

		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_SFLOAT:

		case VK_FORMAT_R32G32B32_UINT:
		case VK_FORMAT_R32G32B32_SINT:
		case VK_FORMAT_R32G32B32_SFLOAT:

		case VK_FORMAT_R32G32B32A32_UINT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			return 4;

		case VK_FORMAT_R64_UINT:
		case VK_FORMAT_R64_SINT:
		case VK_FORMAT_R64_SFLOAT:

		case VK_FORMAT_R64G64_UINT:
		case VK_FORMAT_R64G64_SINT:
		case VK_FORMAT_R64G64_SFLOAT:

		case VK_FORMAT_R64G64B64_UINT:
		case VK_FORMAT_R64G64B64_SINT:
		case VK_FORMAT_R64G64B64_SFLOAT:

		case VK_FORMAT_R64G64B64A64_UINT:
		case VK_FORMAT_R64G64B64A64_SINT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return 8;
	}
	return 0;
}
