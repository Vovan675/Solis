#include "pch.h"
#include "RHITexture.h"
#include "Assets/AssetManager.h"
#include "Utils/Image.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define TINYDDSLOADER_IMPLEMENTATION
#include "tinyddsloader.h"
#include "Utils/Math.h"
#include "Assets/TextureImportSettings.h"

void RHITexture::reload()
{
	std::filesystem::path current = AssetManager::getPath(guid);
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

static Ref<Asset> load_texture(const std::filesystem::path &path)
{
	TextureDescription description{};
	description.format = FORMAT_R8G8B8A8_UNORM;
	description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;

	RHITextureRef texture = gDynamicRHI->createTexture(description);
	texture->load(path.string().c_str());
	return texture;
}

static void cook_texture(const AssetMetadata &metadata, const std::filesystem::path &runtime_path)
{
	Ref<Image> image = new Image(metadata.sourcePath.string().c_str());
	if (metadata.getImportSettings<TextureImportSettings>().generate_mipmaps)
		image->createMipmaps();

	image->save(runtime_path);
}

static const AssetTypeInfo *registered_texture_type = AssetManager::registerType<RHITexture>({
	"Texture", {".dds", ".png", ".jpg", ".jpeg", ".hdr", ".tga"}, load_texture,
	".dds", cook_texture, &Reflected<TextureImportSettings>::getInfo(),
});
