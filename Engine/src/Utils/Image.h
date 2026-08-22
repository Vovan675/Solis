#pragma once
#include "Core/Core.h"
#include "Assets/Asset.h"
#include "RHI/RHIDefinitions.h"

class Image : public Asset
{
public:
	Image(eastl::string path);

	uint32_t getWidth(int mip = 0) const { return width >> mip; }
	uint32_t getHeight(int mip = 0) const { return height >> mip; }
	uint32_t getMipLevels() const { return mip_levels; }
	Format getFormat() const { return format; }

	eastl::vector<uint8_t> &getRawData() { return data; }


	bool isCompressedFormat() const
	{ 
		return format >= FORMAT_BC1 && format <= FORMAT_BC7;
	}

	uint64_t getImageSize(uint32_t mip) const
	{
		uint64_t image_size = getWidth(mip) * getHeight(mip) * getFormatSize(format);

		unsigned int block_width = 4;
		unsigned int block_height = 4;
		if (isCompressedFormat())
		{
			unsigned int block_size = 8;
			unsigned int blocks_width = (getWidth(mip) + block_width - 1) / block_width;
			unsigned int blocks_height = (getHeight(mip) + block_height - 1) / block_height;

			switch (format)
			{
				case FORMAT_BC1:
					block_size = 8;
					break;
				case FORMAT_BC3:
				case FORMAT_BC5:
				case FORMAT_BC7:
					block_size = 16;
					break;
			}
			image_size = blocks_width * blocks_height * block_size;
		}

		return image_size;
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