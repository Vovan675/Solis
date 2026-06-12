#pragma once
#include "FrameGraph/FrameGraph.h"
#include "MeshletPass.h"

class GBufferPass
{
public:
	void addPass(FrameGraph &fg, uint32_t max_draw_calls);

private:
	MeshletPass meshlet_pass;
	RHITextureRef persistent_hiz;

	void import_or_create_hiz(FrameGraph &fg);
	void add_geometry_pass(FrameGraph &fg, uint32_t max_draw_calls);
	void add_traditional_cull_pass(FrameGraph &fg, const MeshletCullDesc &desc);
	void add_traditional_geometry_pass(FrameGraph &fg, uint32_t max_draw_calls);
	void add_hiz_pass(FrameGraph &fg);
};
