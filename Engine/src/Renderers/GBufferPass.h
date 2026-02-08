#pragma once
#include "FrameGraph/FrameGraph.h"
#include "EntityRenderer.h"
#include "Rendering/Renderer.h"

class GBufferPass
{
public:
	void addPass(FrameGraph& fg, uint32_t max_draw_calls);

	EntityRenderer* entity_renderer;

private:
	void addHiZPass(FrameGraph& fg);
	void addCullingPasses(FrameGraph& fg, bool is_late_pass, uint32_t max_draw_calls);
	void addGeometryPass(FrameGraph& fg, uint32_t max_draw_calls);

	void addCounterInitPass(FrameGraph& fg, bool is_late_pass);
	void addInstanceCullingPass(FrameGraph& fg, bool is_late_pass, uint32_t max_draw_calls);
	void addMeshletDispatchArgsPass(FrameGraph& fg, bool is_late_pass);
	void addMeshletCullingPass(FrameGraph& fg, bool is_late_pass, uint32_t max_draw_calls);
	void addGeometryDispatchArgsPass(FrameGraph& fg, bool is_late_pass);
};
