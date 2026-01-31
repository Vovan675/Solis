#pragma once
#include "FrameGraph/FrameGraph.h"
#include "EntityRenderer.h"
#include "Rendering/Renderer.h"

class GBufferPass
{
public:
	GBufferPass();

	void addPass(FrameGraph &fg, uint32_t max_draw_calls_count, RHIBufferRef instances_pass_masks_gpu);

	EntityRenderer *entity_renderer;

private:
	void gpu_pass_cull(FrameGraph &fg, bool is_late, uint32_t max_draw_calls_count, RHIBufferRef instances_pass_masks_gpu);
	void render_pass(FrameGraph &fg, uint32_t max_draw_calls_count);

	RHIShaderRef gbuffer_vertex_shader;
	RHIShaderRef gbuffer_fragment_shader;

	RHIBufferRef draw_indexed_args_gpu;
	RHIBufferRef draw_indexed_count_gpu;
	RHIBufferRef draw_calls_instances_gpu;

	RHIBufferRef indirect_visibility_gpu;
};