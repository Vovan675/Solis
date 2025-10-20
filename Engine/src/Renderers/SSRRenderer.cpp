#include "pch.h"
#include "SSRRenderer.h"
#include "PostProcessingRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"

void SSRRenderer::addPasses(FrameGraph &fg)
{
	fg.addCallbackPass<EmptyData>("SSR Pass",
	[&](RenderPassBuilder &builder, EmptyData &data)
	{
		builder.createTexture(GFXRID(SSR), Renderer::getViewportWidth(), Renderer::getViewportHeight(), FORMAT_R16G16B16A16_UNORM);
		builder.writeTexture(GFXRID(SSR));

		builder.readTexture(GFXRID(FinalNoPostTexture));
		builder.readTexture(GFXRID(GBufferNormal));
		builder.readTexture(GFXRID(GBufferShading));
		builder.readTexture(GFXRID(GBufferDepth));
	},
	[=](const EmptyData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto &ssr = resources.getResource<FrameGraphTexture>(GFXRID(SSR));

		struct UBO
		{
			uint32_t color_tex_id = 0;
			uint32_t normal_tex_id = 0;
			uint32_t shading_tex_id = 0;
			uint32_t depth_tex_id = 0;
		} ubo;
		ubo.color_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(FinalNoPostTexture)).getBindlessId();
		ubo.normal_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(GBufferNormal)).getBindlessId();
		ubo.shading_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(GBufferShading)).getBindlessId();
		ubo.depth_tex_id = resources.getResource<FrameGraphTexture>(GFXRID(GBufferDepth)).getBindlessId();

		cmd_list->setRenderTargets({ssr.texture}, nullptr, -1, 0, true);

		auto &p = gGlobalPipeline;
		p->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/ssr.hlsl", FRAGMENT_SHADER));

		// Uniforms
		gDynamicRHI->setConstantBufferData(0, &ubo, sizeof(UBO));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		p->unbind(cmd_list);
		cmd_list->resetRenderTargets();
	});
}
