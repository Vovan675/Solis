#pragma once
#include "Core/Core.h"
#include "Assets/Asset.h"
#include "RHI/RHIDefinitions.h"
#include "Utils/FileMemory.h"

class Image : public Asset
{
public:
	Image(eastl::string path);
	Image(uint32_t width, uint32_t height, Format format, eastl::vector<uint8_t> data);

	uint32_t getWidth(int mip = 0) const { return std::max(width >> mip, 1u); }
	uint32_t getHeight(int mip = 0) const { return std::max(height >> mip, 1u); }
	uint32_t getMipLevels() const { return mip_levels; }
	Format getFormat() const { return format; }
	bool isCompressed() const { return isCompressedFormat(format); }
	bool isBlockAligned() const { return width % 4 == 0 && height % 4 == 0; }
	bool hasAlpha() const;

	const uint8_t *getRawData() const { return data.empty() ? (const uint8_t *)file.getData() + file_offset : data.data(); }

	uint64_t getImageSize(uint32_t mip) const
	{
		return getFormatSliceSize(format, getWidth(mip), getHeight(mip));
	}

	uint64_t getImageSize() const
	{
		uint64_t image_size = 0;
		for (int i = 0; i < mip_levels; i++)
			image_size += getImageSize(i);
		return image_size;
	}

	void createMipmaps(bool srgb);
	void removeMips(uint32_t first_mip, uint32_t count);
	void convert(Format new_format);

	void load(eastl::string path);

	void save(const std::filesystem::path &path);
private:
	void copy_mapped_file();

	eastl::vector<uint8_t> data;
	FileMemory file;
	uint64_t file_offset = 0;

	uint32_t width;
	uint32_t height;
	uint32_t mip_levels;

	Format format;
};