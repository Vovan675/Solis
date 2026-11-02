#include "pch.h"
#include "PathTracingRenderer.h"
#include "FrameGraph/FrameGraphData.h"
#include "Rendering/GlobalPipeline.h"
#include "Rendering/ShaderStructs.h"
#include "Core/Variables.h"

static uint32_t accumulation_frame = 0;

void PathTracingRenderer::addPass(FrameGraph &fg, Ref<RayTracingScene> rt_scene, uint32_t sun_light_index)
{
	accumulation_frame++;

	if (render_path_tracing_first_frame)
		accumulation_frame = 0;
	render_path_tracing_first_frame = false;

	fg.addCallbackPass("Path Tracing Pass",
	[&](RenderPassBuilder &builder)
	{
		if (builder.createPersistentTexture(GFXRID(PathTraceAccumulation), Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R32G32B32A32_SFLOAT))
			accumulation_frame = 0;
		builder.createTexture(GFXRID(FinalNoPostTexture), Renderer::getRenderWidth(), Renderer::getRenderHeight(), Format::R32G32B32A32_SFLOAT);
		builder.writeUAVTexture(GFXRID(FinalNoPostTexture));
		builder.writeUAVTexture(GFXRID(PathTraceAccumulation));
		builder.readTexture(GFXRID(Sky));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		gGlobalPipeline->setupRayTracing(L"shaders/rt/path_tracing.hlsl");
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Constants
		{
			uint32_t sun_light_index;
			uint32_t accumulation_frame;
			uint32_t environment_tex_id;
			uint32_t output_tex_id;
			uint32_t accumulation_tex_id;
		} constants;
		constants.sun_light_index = sun_light_index;
		constants.accumulation_frame = accumulation_frame;
		constants.environment_tex_id = GFXOPTIONS(sky).enabled ? resources.getReadTexture(GFXRID(Sky)) : 0;
		constants.output_tex_id = resources.getReadWriteTexture(GFXRID(FinalNoPostTexture));
		constants.accumulation_tex_id = resources.getReadWriteTexture(GFXRID(PathTraceAccumulation));
		gDynamicRHI->setConstantBufferData(3, &constants, sizeof(constants));

		cmd_list->dispatchRays(Renderer::getRenderResolution().x, Renderer::getRenderResolution().y, 1);

	});
}
