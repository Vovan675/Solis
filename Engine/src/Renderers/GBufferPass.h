#pragma once
#include "FrameGraph/FrameGraph.h"

class GBufferPass
{
public:
	void addPass(FrameGraph &fg, uint32_t max_draw_calls);

private:
	RHITextureRef hiz_texture;
};
