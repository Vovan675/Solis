#pragma once
#include "FrameGraph/FrameGraph.h"
#include "EntityRenderer.h"
#include "Rendering/Renderer.h"

class GBufferPass
{
public:
	GBufferPass();

	void AddPass(FrameGraph &fg, const std::vector<RenderBatch> &batches);

	EntityRenderer *entity_renderer;

private:
	RHIShaderRef gbuffer_vertex_shader;
	RHIShaderRef gbuffer_fragment_shader;
};