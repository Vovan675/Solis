#include "pch.h"
#include "Image.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <tinyexr.h>
#include <compressonator.h>
#include <glm/gtc/packing.hpp>
#include "Utils/FileMemory.h"
#include "Utils/FileStream.h"

static CMP_FORMAT to_cmp_format(Format format)
{
	switch (format)
	{
		case Format::R8G8B8A8_UNORM:
		case Format::R8G8B8A8_SRGB: return CMP_FORMAT_RGBA_8888;
		case Format::BC1_UNORM:
		case Format::BC1_SRGB: return CMP_FORMAT_BC1;
		case Format::BC3_UNORM:
		case Format::BC3_SRGB: return CMP_FORMAT_BC3;
		case Format::BC5_UNORM: return CMP_FORMAT_BC5;
		case Format::BC7_UNORM:
		case Format::BC7_SRGB: return CMP_FORMAT_BC7;
	}
	return CMP_FORMAT_Unknown;
}

Image::Image(eastl::string path)
{
	load(path);
}

Image::Image(uint32_t width, uint32_t height, Format format, eastl::vector<uint8_t> data)
	: data(eastl::move(data)), width(width), height(height), mip_levels(1), format(format)
{}

bool Image::hasAlpha() const
{
	ENGINE_ASSERT(format == Format::R8G8B8A8_UNORM || format == Format::R8G8B8A8_SRGB);
	const uint8_t *pixels = getRawData();
	for (uint64_t i = 3; i < getImageSize(0); i += 4)
	{
		if (pixels[i] < 255)
			return true;
	}
	return false;
}

void Image::createMipmaps(bool srgb)
{
	ENGINE_ASSERT(format == Format::R32G32B32A32_SFLOAT || format == Format::R8G8B8A8_UNORM || format == Format::R8G8B8A8_SRGB);
	uint32_t texel_size = getFormatInfo(format).bytes_per_block;
	copy_mapped_file();

	auto decode_texel = [&](const uint8_t *texel)
	{
		glm::vec4 color;
		if (format == Format::R32G32B32A32_SFLOAT)
		{
			memcpy(&color, texel, sizeof(color));
		} else if (srgb)
		{
			color = glm::vec4(Math::srgb8ToLinear(texel[0]), Math::srgb8ToLinear(texel[1]), Math::srgb8ToLinear(texel[2]), texel[3] / 255.0f);
		} else
		{
			color = glm::vec4(texel[0], texel[1], texel[2], texel[3]) / 255.0f;
		}
		return color;
	};
	auto encode_texel = [&](uint8_t *texel, glm::vec4 color)
	{
		if (format == Format::R32G32B32A32_SFLOAT)
		{
			memcpy(texel, &color, sizeof(color));
		} else
		{
			for (int i = 0; i < 3; i++)
				texel[i] = srgb ? Math::linearToSRGB8(color[i]) : (uint8_t)glm::round(glm::clamp(color[i], 0.0f, 1.0f) * 255.0f);
			texel[3] = (uint8_t)glm::round(glm::clamp(color.a, 0.0f, 1.0f) * 255.0f);
		}
	};

	// Allocate full size
	mip_levels = std::log2(std::max(width, height)) + 1;
	data.resize(getImageSize());

	// Generate mips
	eastl::vector<glm::vec4> src_mip;
	size_t dst_offset = getImageSize(0);
	for (int mip = 1; mip < mip_levels; mip++)
	{
		int src_width = getWidth(mip - 1);
		int src_height = getHeight(mip - 1);
		int dst_width = getWidth(mip);
		int dst_height = getHeight(mip);
		auto get_src_texel = [&](int x, int y)
		{
			size_t index = (size_t)y * src_width + x;
			return mip == 1 ? decode_texel(&data[index * texel_size]) : src_mip[index];
		};

		eastl::vector<glm::vec4> dst_mip((size_t)dst_width * dst_height);
		for (int y = 0; y < dst_height; y++)
		{
			int y0 = y * 2;
			int y1 = std::min(y * 2 + 1, src_height - 1);
			for (int x = 0; x < dst_width; x++)
			{
				int x0 = x * 2;
				int x1 = std::min(x * 2 + 1, src_width - 1);
				size_t dst_index = (size_t)y * dst_width + x;
				dst_mip[dst_index] = (get_src_texel(x0, y0) + get_src_texel(x1, y0) + get_src_texel(x0, y1) + get_src_texel(x1, y1)) * 0.25f; // Box filter
				encode_texel(&data[dst_offset + dst_index * texel_size], dst_mip[dst_index]);
			}
		}

		// Move to next mip
		dst_offset += getImageSize(mip);
		src_mip = eastl::move(dst_mip);
	}
}

void Image::removeMips(uint32_t first_mip, uint32_t count)
{
	ENGINE_ASSERT(first_mip == 0 || first_mip + count == mip_levels); // Can only remove first mips or last mips
	if (count == 0)
		return;

	copy_mapped_file();
	uint64_t offset = 0;
	for (int mip = 0; mip < first_mip; mip++)
		offset += getImageSize(mip);
	uint64_t size = 0;
	for (int mip = first_mip; mip < first_mip + count; mip++)
		size += getImageSize(mip);

	data.erase(data.begin() + offset, data.begin() + offset + size);
	// If removed first mips, recalculate new size
	if (first_mip == 0)
	{
		width = getWidth(count);
		height = getHeight(count);
	}
	mip_levels -= count;
}

void Image::convert(Format new_format)
{
	CMP_FORMAT src_cmp_format = to_cmp_format(format);
	CMP_FORMAT dst_cmp_format = to_cmp_format(new_format);
	if (format == Format::R32G32B32A32_SFLOAT && new_format == Format::R16G16B16A16_SFLOAT)
	{
		copy_mapped_file();
		size_t value_count = data.size() / sizeof(float);
		eastl::vector<uint8_t> converted_data(value_count * sizeof(uint16_t));
		const float *src = (const float *)data.data();
		uint16_t *dst = (uint16_t *)converted_data.data();
		for (size_t i = 0; i < value_count; i++)
			dst[i] = glm::packHalf1x16(glm::clamp(src[i], -65504.0f, 65504.0f));
		data = eastl::move(converted_data);
	} else if (src_cmp_format != dst_cmp_format)
	{
		copy_mapped_file();
		CMP_CompressOptions options{};
		options.dwSize = sizeof(options);
		options.fquality = 0.05f;

		eastl::vector<uint8_t> converted_data;
		size_t src_offset = 0;
		for (int mip = 0; mip < mip_levels; mip++)
		{
			size_t dst_offset = converted_data.size();
			uint32_t dst_size = getFormatSliceSize(new_format, getWidth(mip), getHeight(mip));
			converted_data.resize(dst_offset + dst_size);

			CMP_Texture src{};
			src.dwSize = sizeof(src);
			src.dwWidth = getWidth(mip);
			src.dwHeight = getHeight(mip);
			src.dwPitch = getFormatRowSize(format, getWidth(mip));
			src.format = src_cmp_format;
			src.dwDataSize = getFormatSliceSize(format, getWidth(mip), getHeight(mip));
			src.pData = data.data() + src_offset;

			CMP_Texture dst{};
			dst.dwSize = sizeof(dst);
			dst.dwWidth = getWidth(mip);
			dst.dwHeight = getHeight(mip);
			dst.format = dst_cmp_format;
			dst.dwDataSize = dst_size;
			dst.pData = converted_data.data() + dst_offset;

			if (CMP_ConvertTexture(&src, &dst, &options, nullptr) != CMP_OK)
				CORE_ERROR("Converting image mip {} to {} failed", mip, getFormatInfo(new_format).name);
			src_offset += src.dwDataSize;
		}
		data = eastl::move(converted_data);
	}
	format = new_format;
}

void Image::load(eastl::string path)
{
	if (!file.open(path, true, 0))
	{
		CORE_ERROR("Can't open image {}", path.c_str());
		return;
	}
	const uint8_t *bytes = (const uint8_t *)file.getData();
	size_t size = file.getSize();

	dds::Header header = dds::read_header(bytes, size);
	bool is_dds = header.is_valid();

	if (is_dds)
	{
		format = fromDXGIFormat(header.format());
		if (format == Format::UNDEFINED || header.data_offset() + header.slice_size() > size)
		{
			CORE_ERROR("Unsupported DDS {}", path.c_str());
			file.close();
			return;
		}

		width = header.width();
		height = header.height();
		mip_levels = header.mip_levels();
		file_offset = header.data_offset();
	} else
	{
		int image_width;
		int image_height;
		int channels;
		void *pixels;
		if (stbi_is_hdr_from_memory(bytes, (int)size))
		{
			pixels = stbi_loadf_from_memory(bytes, (int)size, &image_width, &image_height, &channels, STBI_rgb_alpha);
			format = Format::R32G32B32A32_SFLOAT;
		} else
		{
			pixels = stbi_load_from_memory(bytes, (int)size, &image_width, &image_height, &channels, STBI_rgb_alpha);
			format = Format::R8G8B8A8_UNORM;
		}
		file.close();

		if (!pixels)
		{
			CORE_ERROR("Loading image {} failed: {}", path.c_str(), stbi_failure_reason());
			return;
		}

		width = image_width;
		height = image_height;
		mip_levels = 1;
		data.assign((uint8_t *)pixels, (uint8_t *)pixels + getImageSize());
		stbi_image_free(pixels);
	}
}

void Image::save(const std::filesystem::path &path)
{
	std::filesystem::create_directory(path.parent_path());
	auto extension = path.extension();
	if (extension == ".dds")
	{
		dds::Header header{};
		dds::write_header(&header, getFormatInfo(format).dxgi_format, width, height, mip_levels);

		FileStream stream(path.string().c_str(), std::ofstream::out | std::ofstream::binary);
		stream.write(header);
		stream.writeBytes((const char *)getRawData(), getImageSize());
	} else if (extension == ".png")
	{
		if (!stbi_write_png(path.string().c_str(), width, height, 4, getRawData(), width * 4))
			CORE_ERROR("Saving image {} failed", path.string());
	} else if (extension == ".exr")
	{
		ENGINE_ASSERT(format == Format::R32G32B32A32_SFLOAT);
		const char *error = nullptr;
		if (SaveEXR((const float *)getRawData(), width, height, 4, false, path.string().c_str(), &error) != TINYEXR_SUCCESS)
		{
			CORE_ERROR("Saving image {} failed: {}", path.string(), error);
			FreeEXRErrorMessage(error);
		}
	} else
	{
		CORE_ERROR("Saving image to extension {} not supported", extension.string());
	}
}

void Image::copy_mapped_file()
{
	if (data.empty())
	{
		data.assign(getRawData(), getRawData() + getImageSize());
		file.close();
	}
}
