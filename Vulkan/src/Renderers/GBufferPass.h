#pragma once
#include "RHI/CommandBuffer.h"
#include "FrameGraph/FrameGraph.h"
#include "EntityRenderer.h"

class GBufferPass
{
public:
	GBufferPass();

	void AddPass(FrameGraph &fg);

	EntityRenderer *entity_renderer;

private:
	std::shared_ptr<Shader> gbuffer_vertex_shader;
	std::shared_ptr<Shader> gbuffer_fragment_shader;
};