#include "pch.h"
#include "RHITexture.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define TINYDDSLOADER_IMPLEMENTATION
#include "tinyddsloader.h"
#include "Utils/Math.h"
#include "Assets/AssetManager.h"

void RHITexture::reload()
{
	std::filesystem::path current = AssetManager::getPathFromGUID(asset_handle);
	if (current.empty())
		current = path.c_str();
	if (current.empty())
		return;

	AssetManager::recreateRuntime(current);
	load(current.string().c_str());
}

uint32_t RHITexture::get_block_size(Format format) const
{
	switch (format)
	{
		case FORMAT_BC1:
		case FORMAT_BC3:
		case FORMAT_BC5:
		case FORMAT_BC7: return 4;
	}
	return 0;
}

uint32_t RHITexture::get_block_stride(Format format) const
{
	switch (format)
	{
		case FORMAT_BC1: return 8;
		case FORMAT_BC3:
		case FORMAT_BC5:
		case FORMAT_BC7: return 16;
	}
	return 0;
}

uint32_t RHITexture::get_row_size(Format format, uint32_t width) const
{
	if (isCompressedFormat())
		return Math::divideRoundUp(width, get_block_size(format)) * get_block_stride(format);
	return getFormatSize(format) * width;
}

uint32_t RHITexture::get_slice_size(Format format, uint32_t width, uint32_t height) const
{
	if (isCompressedFormat())
	{
		uint32_t blocks_width = Math::divideRoundUp(width, get_block_size(format));
		uint32_t blocks_height = Math::divideRoundUp(height, get_block_size(format));
		return blocks_width * blocks_height * get_block_stride(format);
	}
	return getFormatSize(format) * width * height;
}
