#include "pch.h"
#include "RHITexture.h"
#include "Assets/AssetManager.h"
#include "Utils/Image.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define TINYDDSLOADER_IMPLEMENTATION
#include "tinyddsloader.h"
#include "Assets/TextureImportSettings.h"

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
	RHITextureRef texture = gDynamicRHI->createTexture(image_to_texture_description(image, format), image.getRawData().data());
	texture->guid = image.guid;
	texture->path = path;
	return texture;
}

void RHITexture::reload()
{
	std::filesystem::path current = AssetManager::getPath(guid);
	if (current.empty())
		current = path.c_str();
	if (current.empty())
		return;

	AssetManager::recreateRuntime(current);
	Image image(current.string().c_str());
	recreate(image_to_texture_description(image, description.format), image.getRawData().data());
	guid = image.guid;
	path = current.string().c_str();
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
	return RHITexture::load(path.string().c_str(), Format::R8G8B8A8_UNORM);
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
