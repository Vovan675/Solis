#pragma once
#include "Core/Core.h"
#include "Assets/Asset.h"
#include "RHI/RHIDefinitions.h"

class Image : public Asset
{
public:
	Image(eastl::string path);
	Image(uint32_t width, uint32_t height, Format format, eastl::vector<uint8_t> data);

	uint32_t getWidth(int mip = 0) const { return width >> mip; }
	uint32_t getHeight(int mip = 0) const { return height >> mip; }
	uint32_t getMipLevels() const { return mip_levels; }
	Format getFormat() const { return format; }

	eastl::vector<uint8_t> &getRawData() { return data; }

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

	void createMipmaps();

	void load(eastl::string path);

	void save(const std::filesystem::path &path);
private:
	eastl::vector<uint8_t> data;
	eastl::string path;

	uint32_t width;
	uint32_t height;
	uint32_t mip_levels;

	Format format;
};