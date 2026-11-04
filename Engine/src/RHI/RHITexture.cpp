#include "pch.h"
#include "RHITexture.h"
#include "Assets/AssetManager.h"
#include "Assets/TextureImportSettings.h"
#include "Utils/Image.h"

static const uint32_t TEXTURE_RUNTIME_VERSION = 1;

static TextureDescription image_to_texture_description(const Image &image, Format format)
{
	TextureDescription description{};
	description.width = image.getWidth();
	description.height = image.getHeight();
	description.mip_levels = image.getMipLevels();
	description.format = format == Format::UNDEFINED || isCompressedFormat(image.getFormat()) ? image.getFormat() : format;
	return description;
}

RHITextureRef RHITexture::load(const char *path, Format format)
{
	Image image(path);
	if (!image.getRawData())
		return nullptr;
	return gDynamicRHI->createTexture(image_to_texture_description(image, format), image.getRawData());
}

void RHITexture::reload()
{
	std::filesystem::path source_path = AssetManager::getPath(guid);
	AssetManager::recreateRuntime(source_path);

	Image image(AssetManager::getRuntimePath(source_path).string().c_str());
	if (image.getRawData())
		recreate(image_to_texture_description(image, Format::UNDEFINED), image.getRawData());
}

void RHITexture::transitState(RHICommandList *cmd_list, ResourceState new_state, uint32_t mip)
{
	bool barriers_required = false;

	size_t difference_mip_index; // Index where mip states start differ
	size_t difference_mip_count; // How many mip states differ

	size_t start_index = mip == ALL_MIPS ? 0 : mip;
	size_t end_index = mip == ALL_MIPS ? description.mip_levels : start_index + 1;
	for (size_t i = start_index; i < end_index; i++)
	{
		if (current_states[i] != new_state || new_state == ResourceState::UNORDERED_ACCESS || new_state == ResourceState::RENDER_TARGET)
		{
			barriers_required = true;
			difference_mip_index = i;
			difference_mip_count = end_index - i;
			break;
		}
	}

	if (!barriers_required) return;

	transit_state(cmd_list, current_states[difference_mip_index], new_state, difference_mip_index, difference_mip_count);

	for (size_t i = difference_mip_index; i < difference_mip_index + difference_mip_count; i++)
		current_states[i] = new_state;
}

RHITextureView *RHITexture::get_view(TextureViewType view_type, uint32_t mip, uint32_t layer)
{
	// Try to find view
	for (auto &view : views)
	{
		const TextureViewDescription &view_desc = view->getDescription();
		if (view_desc.view_type == view_type && view_desc.mip == mip && view_desc.layer == layer)
			return view;
	}

	return views.emplace_back(create_view(TextureViewDescription(this, view_type, mip, layer)));
}

static Ref<Asset> load_texture(const std::filesystem::path &path)
{
	return RHITexture::load(AssetManager::getRuntimePath(path).string().c_str());
}

static void cook_texture(const AssetMetadata &metadata, const std::filesystem::path &runtime_path)
{
	const TextureImportSettings &settings = metadata.getImportSettings<TextureImportSettings>();

	Image image(metadata.sourcePath.string().c_str());
	if (!image.getRawData())
		return;

	bool srgb = settings.srgb && settings.format != TEXTURE_FORMAT_BC5;

	// Mips
	uint32_t max_size = settings.max_size > 0 ? settings.max_size : UINT32_MAX;
	bool is_too_large = std::max(image.getWidth(), image.getHeight()) > max_size;
	if ((settings.generate_mipmaps || is_too_large) && image.getMipLevels() == 1 && !image.isCompressed())
		image.createMipmaps(srgb);

	// Remove first mips for max_size
	uint32_t first_mip = 0;
	while (first_mip + 1 < image.getMipLevels() && std::max(image.getWidth(first_mip), image.getHeight(first_mip)) > max_size)
		first_mip++;
	image.removeMips(0, first_mip);

	// If mips were created only for resizing, remove it
	if (!settings.generate_mipmaps)
		image.removeMips(1, image.getMipLevels() - 1);

	// Format
	Format cooked_format = image.getFormat();
	if (image.getFormat() == Format::R32G32B32A32_SFLOAT)
	{
		cooked_format = Format::R16G16B16A16_SFLOAT;
	} else if (!image.isCompressed() && image.isBlockAligned())
	{
		switch (settings.format)
		{
			case TEXTURE_FORMAT_AUTO:
				cooked_format = image.hasAlpha() ? Format::BC7_UNORM : Format::BC1_UNORM;
				break;
			case TEXTURE_FORMAT_BC1:
				cooked_format = Format::BC1_UNORM;
				break;
			case TEXTURE_FORMAT_BC5:
				cooked_format = Format::BC5_UNORM;
				break;
			case TEXTURE_FORMAT_BC7:
				cooked_format = Format::BC7_UNORM;
				break;
		}
	}

	if (srgb)
	{
		if (cooked_format == Format::R8G8B8A8_UNORM)
			cooked_format = Format::R8G8B8A8_SRGB;
		else if (cooked_format == Format::BC1_UNORM)
			cooked_format = Format::BC1_SRGB;
		else if (cooked_format == Format::BC3_UNORM)
			cooked_format = Format::BC3_SRGB;
		else if (cooked_format == Format::BC7_UNORM)
			cooked_format = Format::BC7_SRGB;
	}

	image.convert(cooked_format);
	image.save(runtime_path);
}

static const AssetTypeInfo *registered_texture_type = AssetManager::registerType<RHITexture>({
	"Texture", {".dds", ".png", ".jpg", ".jpeg", ".hdr", ".tga"}, load_texture,
	".dds", cook_texture, &Reflected<TextureImportSettings>::getInfo(), [](const AssetMetadata &) { return TEXTURE_RUNTIME_VERSION; },
});
