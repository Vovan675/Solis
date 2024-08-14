#include "pch.h"
#include "Texture.h"
#include "VkWrapper.h"
#include "BindlessResources.h"
#include "stb_image.h"
#define TINYDDSLOADER_IMPLEMENTATION
#include "tinyddsloader.h"
#include <filesystem>

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
	this->path = "";
	BindlessResources::removeTexture(this);
	if (sampler != nullptr)
		vkDestroySampler(VkWrapper::device->logicalHandle, sampler, nullptr);

	for (auto &view : image_views)
		view.cleanup();
	image_views.clear();

	if (imageHandle != nullptr && m_Description.destroy_image)
		vkDestroyImage(VkWrapper::device->logicalHandle, imageHandle, nullptr);
	if (allocation != nullptr)
		vmaFreeMemory(VkWrapper::allocator, allocation);
}

void Texture::fill()
{
	cleanup();
	if (m_Description.is_cube == false)
	{
		VkDeviceSize imageSize = m_Description.width * m_Description.height * get_bytes_per_channel() * get_channels_count();
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

		current_layouts.resize(m_Description.mipLevels, TEXTURE_LAYOUT_UNDEFINED);

		VmaAllocationCreateInfo alloc_info{};
		alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		vmaCreateImage(VkWrapper::allocator, &imageInfo, &alloc_info, &imageHandle, &allocation, nullptr);
	
		getImageView();
		create_sampler();
	} else
	{
		VkDeviceSize imageSize = m_Description.width * m_Description.height * get_bytes_per_channel() * get_channels_count() * 6;
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
		imageInfo.usage = m_Description.imageUsageFlags;
		imageInfo.samples = m_Description.numSamples;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		current_layouts.resize(m_Description.mipLevels, TEXTURE_LAYOUT_UNDEFINED);

		VmaAllocationCreateInfo alloc_info{};
		alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		vmaCreateImage(VkWrapper::allocator, &imageInfo, &alloc_info, &imageHandle, &allocation, nullptr);

		getImageView();
		create_sampler();
	}
}

void Texture::fill(const void* sourceData)
{
	cleanup();
	VkDeviceSize image_size;
	if (m_Description.is_cube == false)
	{
		image_size = m_Description.width * m_Description.height * get_bytes_per_channel() * get_channels_count();
		if (m_Description.imageFormat == VK_FORMAT_BC1_RGB_UNORM_BLOCK)
		{
			unsigned int block_width = 4;
			unsigned int block_height = 4;
			unsigned int block_size = 8;
			unsigned int blocks_width = (m_Description.width + block_width - 1) / block_width;
			unsigned int blocks_height = (m_Description.height + block_height - 1) / block_height;
			image_size = blocks_width * blocks_height * block_size;
		}
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

		current_layouts.resize(m_Description.mipLevels, TEXTURE_LAYOUT_UNDEFINED);

		VmaAllocationCreateInfo alloc_info{};
		alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		VkResult res = vmaCreateImage(VkWrapper::allocator, &imageInfo, &alloc_info, &imageHandle, &allocation, nullptr);
	} else
	{
		image_size = m_Description.width * m_Description.height * get_bytes_per_channel() * get_channels_count() * 6;
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

		current_layouts.resize(m_Description.mipLevels, TEXTURE_LAYOUT_UNDEFINED);

		VmaAllocationCreateInfo alloc_info{};
		alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		vmaCreateImage(VkWrapper::allocator, &imageInfo, &alloc_info, &imageHandle, &allocation, nullptr);
	}

	// Create staging buffer
	VkBuffer stagingBuffer;
	VmaAllocation stagingAllocation;
	VkWrapper::createBuffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAllocation);

	// Copy data to staging
	void *data;
	vmaMapMemory(VkWrapper::allocator, stagingAllocation, &data);
	memcpy(data, sourceData, static_cast<size_t>(image_size));
	vmaUnmapMemory(VkWrapper::allocator, stagingAllocation);

	CommandBuffer command_buffer;
	command_buffer.init(true);
	command_buffer.open();

	// Copy staging to image
	transitLayout(command_buffer, TEXTURE_LAYOUT_TRANSFER_DST);
	copy_buffer_to_image(command_buffer, stagingBuffer);

	// Create mipmaps
	generate_mipmaps(command_buffer);

	command_buffer.close();
	command_buffer.waitFence();

	// Destroy unused buffers
	vkDestroyBuffer(VkWrapper::device->logicalHandle, stagingBuffer, nullptr);
	vmaFreeMemory(VkWrapper::allocator, stagingAllocation);

	getImageView();
	create_sampler();
}

void Texture::load(const char *path)
{
	int texWidth, texHeight, texChannels;

	if (m_Description.is_cube == false)
	{
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
			if (format != tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm)
			{
				return;
			}

			m_Description.width = texWidth;
			m_Description.height = texHeight;
			m_Description.imageFormat = VK_FORMAT_BC1_RGB_UNORM_BLOCK;
			m_Description.mipLevels = 1;
			fill(pixels);
		} else {
			stbi_set_flip_vertically_on_load(1);
			pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

			if (!pixels)
			{
				CORE_ERROR("Loading texture error");
				return;
			}

			m_Description.width = texWidth;
			m_Description.height = texHeight;
			m_Description.mipLevels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
			fill(pixels);

			stbi_image_free(pixels);
		}
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
		m_Description.mipLevels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;

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
	this->path = path;
}

void Texture::fill_raw(VkImage image)
{
	cleanup();
	imageHandle = image;
	getImageView();
}

void Texture::transitLayout(CommandBuffer &command_buffer, TextureLayoutType new_layout_type, int mip)
{
	bool barriers_required = false;

	size_t difference_mip_index; // Index where mip layouts start differ
	size_t difference_mip_count; // How many mip layouts differ

	size_t start_index = mip == -1 ? 0 : mip;
	size_t end_index = mip == -1 ? m_Description.mipLevels : start_index + 1;
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

	bool is_depth_texture = m_Description.imageAspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT;
	
	VkImageAspectFlags aspect_flags = is_depth_texture ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	TextureLayoutType current_layout = current_layouts[difference_mip_index];
	VkImageLayout old_layout = get_vk_layout(current_layout);
	VkImageLayout new_layout = get_vk_layout(new_layout_type);
	VkPipelineStageFlags2 src_stage_mask; VkAccessFlags2 src_access_mask;
	VkPipelineStageFlags2 dst_stage_mask; VkAccessFlags2 dst_access_mask;

	switch (current_layout)
	{
		case TEXTURE_LAYOUT_UNDEFINED:
			src_stage_mask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
			src_access_mask = VK_ACCESS_2_MEMORY_READ_BIT;
			break;
		case TEXTURE_LAYOUT_ATTACHMENT:
			if (is_depth_texture)
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
	}

	switch (new_layout_type)
	{
		case TEXTURE_LAYOUT_ATTACHMENT:
			if (is_depth_texture)
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
	}

	int layer_count = m_Description.is_cube ? 6 : 1;

	VkWrapper::cmdImageMemoryBarrier(command_buffer,
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

VkImageView Texture::getImageView(int mip, int face)
{
	// Find already created
	for (const auto &view : image_views)
	{
		if (view.mip == mip && view.face == face)
			return view.image_view;
	}

	// Otherwise create new
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = imageHandle;
	viewInfo.format = m_Description.imageFormat;
	viewInfo.subresourceRange.aspectMask = m_Description.imageAspectFlags;

	// -1 = all mips
	viewInfo.subresourceRange.baseMipLevel = mip == -1 ? 0 : mip;
	viewInfo.subresourceRange.levelCount = mip == -1 ? m_Description.mipLevels : 1;

	if (m_Description.is_cube)
	{
		// -1 = all faces
		if (face == -1)
		{
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 6;
		} else
		{
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.subresourceRange.baseArrayLayer = face;
			viewInfo.subresourceRange.layerCount = 1;
		}
	} else
	{
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.subresourceRange.layerCount = 1;
	}

	VkImageView image_view;
	CHECK_ERROR(vkCreateImageView(VkWrapper::device->logicalHandle, &viewInfo, nullptr, &image_view));

	ImageView view;
	view.mip = mip;
	view.face = face;
	view.image_view = image_view;

	image_views.push_back(view);
	return image_view;
}

VkImageLayout Texture::get_vk_layout(TextureLayoutType layout_type)
{
	bool is_depth_texture = m_Description.imageAspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT;
	switch (layout_type)
	{
		case TEXTURE_LAYOUT_UNDEFINED:
			return VK_IMAGE_LAYOUT_UNDEFINED;
			break;
		case TEXTURE_LAYOUT_ATTACHMENT:
			return is_depth_texture ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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
	}
}

int Texture::get_channels_count() const
{
	switch (m_Description.imageFormat)
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

int Texture::get_bytes_per_channel() const
{
	switch (m_Description.imageFormat)
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

void Texture::generate_mipmaps(CommandBuffer &command_buffer) {
	int faces = m_Description.is_cube ? 6 : 1;
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = imageHandle;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = faces; // Barrier all faces at once
	barrier.subresourceRange.levelCount = 1; // Barrier for only one mip map

	int32_t mipWidth = m_Description.width;
	int32_t mipHeight = m_Description.height;

	transitLayout(command_buffer, TEXTURE_LAYOUT_TRANSFER_DST);
	for (uint32_t i = 1; i < m_Description.mipLevels; i++)
	{
		// Barrier to transfer read
		transitLayout(command_buffer, TEXTURE_LAYOUT_TRANSFER_SRC, i - 1);
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

		vkCmdBlitImage(command_buffer.get_buffer(),
			imageHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			imageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
		transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ, i - 1);
	}
	
	transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ, m_Description.mipLevels - 1);
}

void Texture::copy_buffer_to_image(CommandBuffer &command_buffer, VkBuffer buffer) {
	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = m_Description.is_cube ? 6 : 1;
	region.imageOffset = {0, 0, 0};
	region.imageExtent = {
		m_Description.width,
		m_Description.height,
		1
	};

	vkCmdCopyBufferToImage(command_buffer.get_buffer(), buffer, imageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void Texture::create_sampler()
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = m_Description.filtering;
	samplerInfo.minFilter = m_Description.filtering;
	samplerInfo.addressModeU = m_Description.sampler_address_mode;
	samplerInfo.addressModeV = m_Description.sampler_address_mode;
	samplerInfo.addressModeW = m_Description.sampler_address_mode;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	samplerInfo.anisotropyEnable = m_Description.anisotropy ? VK_TRUE : VK_FALSE;
	samplerInfo.maxAnisotropy = m_Description.anisotropy ? VkWrapper::device->physicalProperties.limits.maxSamplerAnisotropy : 1.0f;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = m_Description.mipLevels;


	CHECK_ERROR(vkCreateSampler(VkWrapper::device->logicalHandle, &samplerInfo, nullptr, &sampler));
}

void Texture::ImageView::cleanup()
{
	if (image_view)
		vkDestroyImageView(VkWrapper::device->logicalHandle, image_view, nullptr);
}