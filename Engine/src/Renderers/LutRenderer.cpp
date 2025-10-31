#include "pch.h"
#include "LutRenderer.h"
#include "RHI/BindlessResources.h"
#include "FrameGraph/FrameGraphRHIResources.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphUtils.h"

LutRenderer::LutRenderer()
{
	TextureDescription desc;
	desc.width = 512;
	desc.height = 512;
	desc.format = FORMAT_R16G16_SFLOAT;
	desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
	desc.sampler_mode = SAMPLER_MODE_CLAMP_TO_EDGE;

	brdf_lut_texture = gDynamicRHI->createTexture(desc);
	brdf_lut_texture->fill();
	brdf_lut_texture->setDebugName("IBL BRDF LUT Image");
	gDynamicRHI->getBindlessResources()->addTexture(brdf_lut_texture);
}

void LutRenderer::addPasses(FrameGraph &fg)
{
	fg.addCallbackPass<EmptyData>("BRDF LUT Pass",
	[&](RenderPassBuilder &builder, EmptyData &data)
	{
		builder.writeTexture(GFXRID(LutBRDF));
	},
	[=](const EmptyData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto brdf_lut = resources.getTexture(GFXRID(LutBRDF));

		cmd_list->setRenderTargets({brdf_lut}, nullptr, -1, 0, true);

		auto &p = gGlobalPipeline;
		p->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/ibl/brdf_lut.hlsl", FRAGMENT_SHADER));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		cmd_list->resetRenderTargets();
	});
}
