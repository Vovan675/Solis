#include "pch.h"
#include "SSRRenderer.h"
#include "PostProcessingRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"

void SSRRenderer::addPasses(FrameGraph &fg)
{
	fg.addCallbackPass("SSR Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(GFXRID(SSR), Renderer::getRenderWidth(), Renderer::getRenderHeight(), FORMAT_R16G16B16A16_UNORM);
		builder.writeTexture(GFXRID(SSR));

		builder.readTexture(GFXRID(FinalNoPostTexture));
		builder.readTexture(GFXRID(GBufferNormal));
		builder.readTexture(GFXRID(GBufferShading));
		builder.readTexture(GFXRID(GBufferDepth));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto ssr = resources.getTexture(GFXRID(SSR));

		struct UBO
		{
			uint32_t color_tex_id = 0;
			uint32_t normal_tex_id = 0;
			uint32_t shading_tex_id = 0;
			uint32_t depth_tex_id = 0;
		} ubo;
		ubo.color_tex_id = resources.getReadTexture(GFXRID(FinalNoPostTexture));
		ubo.normal_tex_id = resources.getReadTexture(GFXRID(GBufferNormal));
		ubo.shading_tex_id = resources.getReadTexture(GFXRID(GBufferShading));
		ubo.depth_tex_id = resources.getReadTexture(GFXRID(GBufferDepth));

		cmd_list->setRenderTargets({ssr}, nullptr, -1, 0, true);

		auto &p = gGlobalPipeline;
		p->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/ssr.hlsl", FRAGMENT_SHADER));

		// Uniforms
		gDynamicRHI->setConstantBufferData(0, &ubo, sizeof(UBO));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		cmd_list->resetRenderTargets();
	});
}
