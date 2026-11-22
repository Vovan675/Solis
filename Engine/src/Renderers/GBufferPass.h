#pragma once
#include "FrameGraph/FrameGraph.h"

class GBufferPass
{
public:
	void addPass(FrameGraph &fg, uint32_t view_id, uint32_t max_draw_calls, bool pick);
	void addDecalPass(FrameGraph &fg, uint32_t view_id, uint32_t max_draw_calls, bool pick);
};
