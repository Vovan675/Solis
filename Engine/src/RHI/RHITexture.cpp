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
#include "Rendering/GlobalPipeline.h"

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

void RHITexture::load(const char *path)
{
	Image image(path);
	guid = image.guid;

	description.width = image.getWidth();
	description.height = image.getHeight();
	description.mip_levels = image.getMipLevels();
	if (description.format == FORMAT_UNDEFINED || image.isCompressedFormat())
		description.format = image.getFormat();
	fill(image.getRawData().data());

	this->path = path;
}

void RHITexture::loadEquirectangularCubemap(const char *path)
{
	TextureDescription desc{};
	desc.usage_flags = TEXTURE_USAGE_ATTACHMENT | TEXTURE_USAGE_TRANSFER_SRC | TEXTURE_USAGE_TRANSFER_DST;
	RHITextureRef equirect_texture = gDynamicRHI->createTexture(desc);
	equirect_texture->load(path);

	description.is_cube = true;
	description.format = FORMAT_R32G32B32A32_SFLOAT;
	description.width = equirect_texture->getHeight();
	description.height = equirect_texture->getHeight();
	description.mip_levels = 1;

	fill();
	RHICommandList *cmd_list = gDynamicRHI->getCmdList();

	transitLayout(cmd_list, TEXTURE_LAYOUT_GENERAL);

	gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/equirect_to_cubemap.hlsl", COMPUTE_SHADER));
	gGlobalPipeline->flushAndBind(cmd_list);

	equirect_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_SHADER_READ);
	struct Uniforms
	{
		uint32_t equirect_tex_id;
		uint32_t output_tex_id;
	} uniforms;
	uniforms.equirect_tex_id = equirect_texture->getShaderResourceView()->getBindlessIndex();
	uniforms.output_tex_id = getUnorderedAccessView()->getBindlessIndex();
	gDynamicRHI->setConstantBufferData(1, &uniforms, sizeof(uniforms));

	cmd_list->dispatch(getWidth() / 32, getHeight() / 32, 6);

	transitLayout(cmd_list, TEXTURE_LAYOUT_SHADER_READ);

	this->path = path;
}

void RHITexture::transitLayout(RHICommandList *cmd_list, TextureLayoutType new_layout_type, int mip)
{
	bool barriers_required = false;

	size_t difference_mip_index; // Index where mip layouts start differ
	size_t difference_mip_count; // How many mip layouts differ

	size_t start_index = mip == -1 ? 0 : mip;
	size_t end_index = mip == -1 ? description.mip_levels : start_index + 1;
	for (size_t i = start_index; i < end_index; i++)
	{
		if (current_layouts[i] != new_layout_type || new_layout_type == TEXTURE_LAYOUT_UAV || new_layout_type == TEXTURE_LAYOUT_ATTACHMENT)
		{
			barriers_required = true;
			difference_mip_index = i;
			difference_mip_count = end_index - i;
			break;
		}
	}

	if (!barriers_required) return;

	transit_layout(cmd_list, current_layouts[difference_mip_index], new_layout_type, difference_mip_index, difference_mip_count);

	for (size_t i = difference_mip_index; i < difference_mip_index + difference_mip_count; i++)
		current_layouts[i] = new_layout_type;
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
