#include "pch.h"
#include "Texture.h"
#include "VkWrapper.h"
#include "BindlessResources.h"
#include "stb_image.h"

Texture::Texture(TextureDescription description)
	: m_Description(description)
{

}

Texture::~Texture()
{
	cleanup();
}

void Texture::cleanup()
{
	BindlessResources::removeTexture(this);
	if (sampler != nullptr)
		vkDestroySampler(VkWrapper::device->logicalHandle, sampler, nullptr);
	if (imageView != nullptr)
		vkDestroyImageView(VkWrapper::device->logicalHandle, imageView, nullptr);
	if (imageHandle != nullptr && m_Description.destroy_image)
		vkDestroyImage(VkWrapper::device->logicalHandle, imageHandle, nullptr);
	if (allocation != nullptr)
		vmaFreeMemory(VkWrapper::allocator, allocation);
}

void Texture::fill()
{
	cleanup();
	VkDeviceSize imageSize = m_Description.width * m_Description.height * 4;
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = m_Description.width;
	imageInfo.extent.height = m_Description.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = m_Description.mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = m_Description.imageFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = m_Description.imageUsageFlags;
	imageInfo.samples = m_Description.numSamples;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo alloc_info{};
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vmaCreateImage(VkWrapper::allocator, &imageInfo, &alloc_info, &imageHandle, &allocation, nullptr);
	
	create_image_view();
	create_sampler();
}

void Texture::fill(const void* sourceData)
{
	cleanup();
	if (m_Description.is_cube == false)
	{
		VkDeviceSize imageSize = m_Description.width * m_Description.height * 4;
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = m_Description.width;
		imageInfo.extent.height = m_Description.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = m_Description.mipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = m_Description.imageFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | m_Description.imageUsageFlags;
		imageInfo.samples = m_Description.numSamples;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo alloc_info{};
		alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		vmaCreateImage(VkWrapper::allocator, &imageInfo, &alloc_info, &imageHandle, &allocation, nullptr);

		// Create staging buffer
		VkBuffer stagingBuffer;
		VmaAllocation stagingAllocation;
		VkWrapper::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAllocation);

		// Copy data to staging
		void* data;
		vmaMapMemory(VkWrapper::allocator, stagingAllocation, &data);
		memcpy(data, sourceData, static_cast<size_t>(imageSize));
		vmaUnmapMemory(VkWrapper::allocator, stagingAllocation);

		// Copy staging to image
		transition_image_layout(imageHandle, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copy_buffer_to_image(stagingBuffer, imageHandle, m_Description.width, m_Description.height);
		//TransitionImageLayout(imageHandle, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(VkWrapper::device->logicalHandle, stagingBuffer, nullptr);
		vmaFreeMemory(VkWrapper::allocator, stagingAllocation);

		generate_mipmaps();

		create_image_view();
		create_sampler();
	} else
	{
		VkDeviceSize imageSize = m_Description.width * m_Description.height * 4 * 6;
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = m_Description.width;
		imageInfo.extent.height = m_Description.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = m_Description.mipLevels;
		imageInfo.arrayLayers = 6; // 6 faces
		imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		imageInfo.format = m_Description.imageFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | m_Description.imageUsageFlags;
		imageInfo.samples = m_Description.numSamples;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo alloc_info{};
		alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		vmaCreateImage(VkWrapper::allocator, &imageInfo, &alloc_info, &imageHandle, &allocation, nullptr);

		// Create staging buffer
		VkBuffer stagingBuffer;
		VmaAllocation stagingAllocation;
		VkWrapper::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAllocation);

		// Copy data to staging
		void *data;
		vmaMapMemory(VkWrapper::allocator, stagingAllocation, &data);
		memcpy(data, sourceData, static_cast<size_t>(imageSize));
		vmaUnmapMemory(VkWrapper::allocator, stagingAllocation);

		// Copy staging to image
		transition_image_layout(imageHandle, VK_FORMAT_R32G32B32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copy_buffer_to_image(stagingBuffer, imageHandle, m_Description.width, m_Description.height);
		transition_image_layout(imageHandle, VK_FORMAT_R32G32B32_SFLOAT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(VkWrapper::device->logicalHandle, stagingBuffer, nullptr);
		vmaFreeMemory(VkWrapper::allocator, stagingAllocation);

		//generate_mipmaps();

		create_image_view();
		create_sampler();
	}
}

void Texture::load(const char *path)
{
	int texWidth, texHeight, texChannels;

	if (m_Description.is_cube == false)
	{
		stbi_set_flip_vertically_on_load(1);
		stbi_uc *pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

		if (!pixels)
		{
			CORE_ERROR("Loading texture error");
		}

		m_Description.width = texWidth;
		m_Description.height= texHeight;
		m_Description.mipLevels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
		fill(pixels);

		stbi_image_free(pixels);
	} else
	{
		stbi_set_flip_vertically_on_load(0);
		stbi_uc *pos_x = stbi_load((std::string(path) + "posx.jpg").c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!pos_x)
			CORE_ERROR("Loading texture error");

		stbi_uc *neg_x = stbi_load((std::string(path) + "negx.jpg").c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!neg_x)
			CORE_ERROR("Loading texture error");

		stbi_uc *pos_y = stbi_load((std::string(path) + "posy.jpg").c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!pos_y)
			CORE_ERROR("Loading texture error");

		stbi_uc *neg_y = stbi_load((std::string(path) + "negy.jpg").c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!neg_y)
			CORE_ERROR("Loading texture error");

		stbi_uc *pos_z = stbi_load((std::string(path) + "posz.jpg").c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!pos_y)
			CORE_ERROR("Loading texture error");

		stbi_uc *neg_z = stbi_load((std::string(path) + "negz.jpg").c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!neg_y)
			CORE_ERROR("Loading texture error");

		m_Description.width = texWidth;
		m_Description.height = texHeight;
		m_Description.mipLevels = 1;

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
	}
}

void Texture::fill_raw(VkImage image)
{
	cleanup();
	imageHandle = image;
	create_image_view();
}

void Texture::generate_mipmaps() {
	VkCommandBuffer commandBuffer = VkWrapper::beginSingleTimeCommands();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = imageHandle;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = m_Description.width;
	int32_t mipHeight = m_Description.height;

	for (uint32_t i = 1; i < m_Description.mipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0; 
		blit.dstSubresource.layerCount = 1;
		vkCmdBlitImage(commandBuffer,
			imageHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			imageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = m_Description.mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	VkWrapper::endSingleTimeCommands(commandBuffer);
}

void Texture::transition_image_layout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkCommandBuffer commandBuffer = VkWrapper::beginSingleTimeCommands();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = m_Description.mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = m_Description.is_cube ? 6 : 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	VkWrapper::endSingleTimeCommands(commandBuffer);
}

void Texture::copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkCommandBuffer commandBuffer = VkWrapper::beginSingleTimeCommands();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = m_Description.is_cube ? 6 : 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	VkWrapper::endSingleTimeCommands(commandBuffer);
}


void Texture::create_image_view()
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = imageHandle;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = m_Description.imageFormat;
	viewInfo.subresourceRange.aspectMask = m_Description.imageAspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	
	if (m_Description.is_cube)
	{
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewInfo.subresourceRange.layerCount = 6;
	}

	CHECK_ERROR(vkCreateImageView(VkWrapper::device->logicalHandle, &viewInfo, nullptr, &imageView));
}

void Texture::create_sampler()
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = VkWrapper::device->physicalProperties.limits.maxSamplerAnisotropy;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = (float)m_Description.mipLevels / 2;
	samplerInfo.maxLod = m_Description.mipLevels;


	CHECK_ERROR(vkCreateSampler(VkWrapper::device->logicalHandle, &samplerInfo, nullptr, &sampler));
}
