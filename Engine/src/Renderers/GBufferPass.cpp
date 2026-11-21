#include "pch.h"
#include "GBufferPass.h"
#include "Rendering/Renderer.h"
#include "OpaqueGeometryPass.h"
#include "FrameGraph/FrameGraphData.h"

void GBufferPass::addPass(FrameGraph &fg, uint32_t view_id, uint32_t max_draw_calls)
{
	OpaqueGeometryPass opaque;

	OpaqueGeometryPass::RenderView view;
	view.view_id = view_id;
	view.pass_mask = PASS_MASK_GBUFFER;
	view.instance_count = max_draw_calls;
	view.render_size = Renderer::getRenderResolution();
	view.hiz = GFXRID(HiZTexture);
	view.hiz_description = HiZ::createDescription(Renderer::getRenderResolution());
	view.layer = 0;
	view.use_two_pass_occlusion = true;
	view.cull_mode = CullMode::NONE;
	view.shaders = OpaqueGeometryPass::ShaderSet::fromFile(L"shaders/gbuffer.hlsl");

	OpaqueGeometryPass::GBufferOutput output;
	output.albedo = GFXRID(GBufferAlbedo);
	output.normal = GFXRID(GBufferNormal);
	output.shading = GFXRID(GBufferShading);
	output.emissive = GFXRID(GBufferEmissive);
	output.motion_vectors = GFXRID(MotionVectors);
	output.depth = GFXRID(GBufferDepth);

	opaque.renderGBuffer(fg, view, output);
}

void GBufferPass::addDecalPass(FrameGraph &fg, uint32_t view_id, uint32_t max_draw_calls)
{
	OpaqueGeometryPass opaque;

	OpaqueGeometryPass::RenderView view;
	view.view_id = view_id;
	view.pass_mask = PASS_MASK_DECAL;
	view.instance_count = max_draw_calls;
	view.layer = 0;
	view.depth_write = false;
	view.depth_func = CompareFunc::GREATER_EQUAL;
	view.blend.enabled = true;
	view.cull_mode = CullMode::NONE;
	view.shaders.traditional_vertex_shader = gDynamicRHI->createShader(L"shaders/gbuffer.hlsl", ShaderType::VERTEX, "VSMainTraditional");
	view.shaders.pixel_shader = gDynamicRHI->createShader(L"shaders/gbuffer.hlsl", ShaderType::FRAGMENT, "PSMainDecal");

	OpaqueGeometryPass::DecalOutput output;
	output.albedo = GFXRID(GBufferAlbedo);
	output.shading = GFXRID(GBufferShading);
	output.depth = GFXRID(GBufferDepth);

	opaque.renderDecals(fg, view, output);
}
