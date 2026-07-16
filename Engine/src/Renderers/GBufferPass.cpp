#include "pch.h"
#include "GBufferPass.h"
#include "Rendering/Renderer.h"
#include "OpaqueGeometryPass.h"
#include "FrameGraph/FrameGraphData.h"

void GBufferPass::addPass(FrameGraph &fg, uint32_t max_draw_calls)
{
	OpaqueGeometryPass opaque;

	HiZ::createOrImport(fg, hiz_texture, GFXRID(HiZTexture), Renderer::getRenderResolution(), 1);

	OpaqueGeometryPass::RenderView view;
	view.view_projection = Renderer::getCamera()->getViewProj();
	view.pass_mask = PASS_MASK_GBUFFER;
	view.instance_count = max_draw_calls;
	view.view_id = 0;
	view.render_size = Renderer::getRenderResolution();
	view.hiz = GFXRID(HiZTexture);
	view.layer = 0;
	view.use_two_pass_occlusion = true;
	view.cull_mode = CULL_MODE_NONE;
	view.use_reverse_z = true;
	view.shaders = OpaqueGeometryPass::ShaderSet::fromFile(L"shaders/gbuffer.hlsl");

	OpaqueGeometryPass::GBufferOutput output;
	output.albedo = GFXRID(GBufferAlbedo);
	output.normal = GFXRID(GBufferNormal);
	output.shading = GFXRID(GBufferShading);
	output.motion_vectors = GFXRID(MotionVectors);
	output.depth = GFXRID(GBufferDepth);

	opaque.renderGBuffer(fg, view, output);
}
