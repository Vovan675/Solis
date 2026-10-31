#pragma once
#include "FrameGraph/FrameGraph.h"

class Image;

namespace UtilityPasses
{
	enum BlitBlend
	{
		BLIT_BLEND_OPAQUE,
		BLIT_BLEND_ALPHA,
	};

	void addBlitPass(FrameGraph &fg, GraphicsResourceName src, GraphicsResourceName dst, BlitBlend blend = BLIT_BLEND_OPAQUE, const wchar_t *shader = L"shaders/utility/blit.hlsl");
	void addDepthBlitPass(FrameGraph &fg, GraphicsResourceName src, GraphicsResourceName dst, glm::ivec2 dst_size);
	void addReadbackPass(FrameGraph &fg, GraphicsResourceName texture, eastl::function<void(Ref<Image>)> callback);
}
