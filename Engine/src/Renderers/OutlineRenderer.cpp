#include "pch.h"
#include "OutlineRenderer.h"
#include "OpaqueGeometryPass.h"
#include "Rendering/Renderer.h"
#include "Rendering/GlobalPipeline.h"
#include "FrameGraph/FrameGraphData.h"

void OutlineRenderer::addPasses(FrameGraph &fg, uint32_t view_id, uint32_t max_draw_calls)
{
	OpaqueGeometryPass opaque;

	OpaqueGeometryPass::RenderView view;
	view.view_id = view_id;
	view.pass_mask = PASS_MASK_OUTLINE;
	view.instance_count = max_draw_calls;
	view.render_size = Renderer::getOutputResolution();
	view.use_two_pass_occlusion = false;
	view.cull_mode = CullMode::NONE;
	view.shaders = OpaqueGeometryPass::ShaderSet::fromFile(L"shaders/gbuffer.hlsl", {{"OUTLINE", "1"}}, "PSMainOutline");

	OpaqueGeometryPass::DepthOutput output;
	output.depth = GFXRID(OutlineDepth);

	opaque.renderDepth(fg, view, output);

	fg.addCallbackPass("Outline",
	[](RenderPassBuilder &builder)
	{
		builder.readTexture(GFXRID(OutlineDepth));
		builder.readTexture(GFXRID(GBufferDepth));
		builder.writeTexture(GFXRID(FinalTexture));
	},
	[](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		cmd_list->setRenderTargets({resources.getTexture(GFXRID(FinalTexture))}, nullptr, 0, 0, false);

		gGlobalPipeline->setupGraphicsPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/quad.hlsl", ShaderType::VERTEX), gDynamicRHI->createShader(L"shaders/outline.hlsl", ShaderType::FRAGMENT), VertexInputsDescription{}, true, false, CullMode::BACK);
		gGlobalPipeline->flushAndBind(cmd_list);

		struct
		{
			uint32_t outline_depth_tex_id;
			uint32_t scene_depth_tex_id;
		} constants;
		constants.outline_depth_tex_id = resources.getReadTexture(GFXRID(OutlineDepth));
		constants.scene_depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->drawInstanced(6, 1, 0, 0);
		cmd_list->resetRenderTargets();
	});
}
