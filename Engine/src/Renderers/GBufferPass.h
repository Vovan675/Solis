#pragma once
#include "FrameGraph/FrameGraph.h"
#include "MeshletPass.h"

class GBufferPass
{
public:
	void addPass(FrameGraph &fg, uint32_t max_draw_calls);

private:
	MeshletPass meshlet_pass;

	void addGeometryPass(FrameGraph &fg, uint32_t max_draw_calls);
	void addHiZPass(FrameGraph &fg);
};
