#pragma once
#include "RHI/CommandBuffer.h"
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
	std::shared_ptr<Shader> gbuffer_vertex_shader;
	std::shared_ptr<Shader> gbuffer_fragment_shader;
};