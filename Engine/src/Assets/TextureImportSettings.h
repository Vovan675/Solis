#pragma once
#include "Core/ReflectionSerialization.h"

enum TextureFormat
{
	TEXTURE_FORMAT_AUTO = 0,
	TEXTURE_FORMAT_BC1,
	TEXTURE_FORMAT_BC5,
	TEXTURE_FORMAT_BC7,
	TEXTURE_FORMAT_RGBA8,
};
inline const char *const texture_format_items[] = {"Auto (BC1, BC7 with alpha)", "BC1 (RGB)", "BC5 (RG, normal maps)", "BC7 (RGBA)", "RGBA8 (uncompressed)"};

struct TextureImportSettings
{
	TextureFormat format = TEXTURE_FORMAT_AUTO;
	bool srgb = true;
	bool generate_mipmaps = true;
	uint32_t max_size = 0;
};

REFLECT_BEGIN(TextureImportSettings)
	REFLECT_FIELD(format).items(texture_format_items),
	REFLECT_FIELD(srgb).label("sRGB").EDIT_IF(owner.format != TEXTURE_FORMAT_BC5),
	REFLECT_FIELD(generate_mipmaps),
	REFLECT_FIELD(max_size).range(0.0f, 8192.0f),
REFLECT_END()
