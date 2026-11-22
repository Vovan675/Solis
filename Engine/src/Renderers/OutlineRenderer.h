#pragma once
#include "FrameGraph/FrameGraph.h"

class OutlineRenderer
{
public:
	void addPasses(FrameGraph &fg, uint32_t view_id, uint32_t max_draw_calls);
};
