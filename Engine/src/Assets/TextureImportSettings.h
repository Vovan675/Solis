#pragma once
#include "Core/ReflectionSerialization.h"

struct TextureImportSettings
{
	bool generate_mipmaps = true;
};

REFLECT_BEGIN(TextureImportSettings)
	REFLECT_FIELD(generate_mipmaps),
REFLECT_END()
