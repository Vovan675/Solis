#include "pch.h"
#include "Image.h"
#include <tinyddsloader.h>
#define TINYDDS_IMPLEMENTATION
#include <tinydds.h>
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <tinyexr.h>
#include <compressonator.h>
#include "dds_helpers.h"
#include "dds.h"
#include "Utils/FileStream.h"
#include "Assets/AssetManager.h"

static CMP_FORMAT to_cmp_format(Format format)
{
	switch(format)
	{
		case Format::R8G8B8A8_SRGB: return CMP_FORMAT_RGBA_8888;
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

void Image::createMipmaps()
{
	// Mipmaps generation for compressed formats is not supported now
	if (isCompressedFormat(format))
		return;
	CMP_MipSet mipSetIn{};
	mipSetIn.m_nWidth = width;
	mipSetIn.m_nHeight = height;
	mipSetIn.m_format = CMP_FORMAT_RGBA_8888;
	mipSetIn.m_nMipLevels = 1;
	mipSetIn.pData = data.data();
	mipSetIn.dwDataSize = data.size();

	CMP_INT requestLevel = CMP_CalcMaxMipLevel(mipSetIn.m_nHeight, mipSetIn.m_nWidth, true);
	CMP_INT nMinSize = CMP_CalcMinMipSize(mipSetIn.m_nHeight, mipSetIn.m_nWidth, requestLevel);

	CMP_MipSet srcMipSet = {};
	CMP_CreateMipSet(&srcMipSet, width, height, 1, CF_8bit, TT_2D);
	memcpy(srcMipSet.pData, data.data(), srcMipSet.dwDataSize);

	CMP_GenerateMIPLevels(&srcMipSet, nMinSize);

	mip_levels = srcMipSet.m_nMipLevels;
	data.resize(getImageSize());

	size_t offset = 0;
	for (int mip = 0; mip < mip_levels; mip++)
	{
		CMP_MipLevel *level = nullptr;
		CMP_GetMipLevel(&level, &srcMipSet, mip, 0);
		memcpy(data.data() + offset, level->m_pdwData, level->m_dwLinearSize);
		offset += level->m_dwLinearSize;
	}

	CMP_FreeMipSet(&mipSetIn);
}

void Image::load(eastl::string path)
{
	// Load from path / or from guid
	// 1. check if registered as asset
	// 2. if registered (already imported), then just open runtime file
	// 3. if not registered (not imported), then create runtime and open runtime
	// If path is already path to runtime, then just load it

	auto runtime_path = std::filesystem::path(path.c_str());

	if (AssetManager::hasValidRuntime(path.c_str()))
	{
		runtime_path = AssetManager::getRuntimePath(path.c_str()).string();
	}
	
	std::filesystem::path tex_path(runtime_path);
	eastl::string ext = tex_path.extension().string().c_str();
	void *pixels;
	if (ext == ".runtime")
	{
		// TODO:
	} else if (ext == ".dds")
	{
		tinyddsloader::DDSFile file;
		tinyddsloader::Result result = file.Load(runtime_path.string().c_str());
		if (result != tinyddsloader::Success)
		{
			assert(false);
			CORE_ERROR("Loading texture error");
			return;
		}

		//file.Flip();
		width = file.GetWidth();
		height = file.GetHeight();
		mip_levels = file.GetMipCount();

		auto dxgi_format = file.GetFormat();
		if (dxgi_format == tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_UNorm || dxgi_format == tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_UNorm)
			format = Format::R8G8B8A8_UNORM;
		else if (dxgi_format == tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_UNorm_SRGB || dxgi_format == tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_UNorm_SRGB)
			format = Format::R8G8B8A8_SRGB;
		else if (dxgi_format == tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm)
			format = Format::BC1_UNORM;
		else if (dxgi_format == tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm_SRGB)
			format = Format::BC1_SRGB;
		else if (dxgi_format == tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm)
			format = Format::BC3_UNORM;
		else if (dxgi_format == tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm_SRGB)
			format = Format::BC3_SRGB;
		else if (dxgi_format == tinyddsloader::DDSFile::DXGIFormat::BC5_UNorm)
			format = Format::BC5_UNORM;
		else if (dxgi_format == tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm)
			format = Format::BC7_UNORM;
		else if (dxgi_format == tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm_SRGB)
			format = Format::BC7_SRGB;
		else
			CORE_ERROR("Invalid texture format");


		data.clear();
		data.reserve(getImageSize());
		for (int i = 0; i < mip_levels; i++)
		{
			auto *img_data = file.GetImageData(i, 0);
			uint8_t *bytes = (uint8_t *)img_data->m_mem;
			data.insert(data.end(), bytes, bytes + getImageSize(i));
		}
	} else
	{
		int tex_width, tex_height, tex_channels;

		if (stbi_is_hdr(path.c_str()))
		{
			pixels = stbi_loadf(path.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
			format = Format::R32G32B32A32_SFLOAT;
		} else
		{
			pixels = stbi_load(path.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);
			format = Format::R8G8B8A8_SRGB;
		}

		if (!pixels)
		{
			CORE_ERROR("Loading texture error");
			return;
		}

		width = tex_width;
		height = tex_height;
		mip_levels = 1;

		uint8_t *bytes = (uint8_t *)pixels;
		data.assign(bytes, bytes + getImageSize());

		stbi_image_free(pixels);
	}
	this->path = runtime_path.string().c_str();
}

void Image::save(const std::filesystem::path &path)
{
	std::filesystem::create_directory(path.parent_path());
	auto extension = path.extension();
	if (extension == ".runtime")
	{
		// Save to native format
		FileStream stream(path.string().c_str(), std::ofstream::out | std::ofstream::binary);
		stream.write(width);
		stream.write(height);
		stream.write(mip_levels);
		stream.write(format);
		stream.write(data);
	} else if (extension == ".dds")
	{
		dds::DXGI_FORMAT dds_format;

		switch (format)
		{
			case Format::R8_UNORM:
				break;
			case Format::R8G8_UNORM:
				break;
			case Format::R8G8B8A8_UNORM:
				dds_format = dds::DXGI_FORMAT_R8G8B8A8_UNORM;
				break;
			case Format::R8G8B8A8_SRGB:
				dds_format = dds::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				break;
			case Format::R16_UNORM:
				break;
			case Format::R16G16_UNORM:
				break;
			case Format::R16G16_SFLOAT:
				break;
			case Format::R16G16B16A16_UNORM:
				break;
			case Format::R16G16B16A16_SFLOAT:
				break;
			case Format::R32_UINT:
				break;
			case Format::R32_SFLOAT:
				break;
			case Format::R32G32_SFLOAT:
				break;
			case Format::R32G32B32_SFLOAT:
				break;
			case Format::R32G32B32A32_SFLOAT:
				break;
			case Format::D32_SFLOAT_S8_UINT:
				break;
			case Format::R11G11B10_UFLOAT:
				break;
			case Format::BC1_UNORM:
				dds_format = dds::DXGI_FORMAT_BC1_UNORM;
				break;
			case Format::BC1_SRGB:
				dds_format = dds::DXGI_FORMAT_BC1_UNORM_SRGB;
				break;
			case Format::BC3_UNORM:
				dds_format = dds::DXGI_FORMAT_BC3_UNORM;
				break;
			case Format::BC3_SRGB:
				dds_format = dds::DXGI_FORMAT_BC3_UNORM_SRGB;
				break;
			case Format::BC5_UNORM:
				dds_format = dds::DXGI_FORMAT_BC5_UNORM;
				break;
			case Format::BC7_UNORM:
				dds_format = dds::DXGI_FORMAT_BC7_UNORM;
				break;
			case Format::BC7_SRGB:
				dds_format = dds::DXGI_FORMAT_BC7_UNORM_SRGB;
				break;
			default:
				assert(false);
				break;
		}

		eastl::vector<uint8_t> dds_data;
		dds_data.resize(sizeof(dds::Header) + data.size());
		dds::write_header(dds_data.data(), dds_format, width, height, mip_levels, 1, false, 0);
		memcpy(dds_data.data() + sizeof(dds::Header), data.data(), data.size());

		FileStream stream(path.string().c_str(), std::ofstream::out | std::ofstream::binary);
		stream.writeBytes((const char *)dds_data.data(), dds_data.size());
	} else if (extension == ".png")
	{
		if (!stbi_write_png(path.string().c_str(), width, height, 4, data.data(), width * 4))
			CORE_ERROR("Saving image {} failed", path.string());
	} else if (extension == ".exr")
	{
		ENGINE_ASSERT(format == Format::R32G32B32A32_SFLOAT);
		const char *error = nullptr;
		if (SaveEXR((const float *)data.data(), width, height, 4, false, path.string().c_str(), &error) != TINYEXR_SUCCESS)
		{
			CORE_ERROR("Saving image {} failed: {}", path.string(), error);
			FreeEXRErrorMessage(error);
		}
	} else
	{
		CORE_ERROR("Saving image to extension {} not supported", extension.string());
	}
}
