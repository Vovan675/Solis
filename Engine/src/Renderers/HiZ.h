#pragma once
#include "FrameGraph/FrameGraph.h"
#include "RHI/RHITexture.h"

// Utility pass for creating HiZ. The result is always standard-z (max-reduced).
class HiZ
{
public:
	static void createOrImport(FrameGraph &fg, RHITextureRef &texture, GraphicsResourceName name, glm::ivec2 size, uint32_t layers = 1);
	static void build(FrameGraph &fg, GraphicsResourceName hiz, GraphicsResourceName depth, uint32_t layer = 0, bool is_depth_reverse_z = true);
};
