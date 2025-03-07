#include "pch.h"
#include "LutRenderer.h"
#include "BindlessResources.h"
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
	LutData &lut_data = fg.getBlackboard().get<LutData>();

	lut_data = fg.addCallbackPass<LutData>("BRDF LUT Pass",
	[&](RenderPassBuilder &builder, LutData &data)
	{
		data.brdf_lut = builder.write(lut_data.brdf_lut);
	},
	[=](const LutData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		FrameGraphTexture &brdf_lut = resources.getResource<FrameGraphTexture>(data.brdf_lut);

		cmd_list->setRenderTargets({brdf_lut.texture}, nullptr, -1, 0, true);

		auto &p = gGlobalPipeline;
		p->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/ibl/brdf_lut.hlsl", FRAGMENT_SHADER));

		// Render quad
		cmd_list->drawInstanced(6, 1, 0, 0);

		p->unbind(cmd_list);
		cmd_list->resetRenderTargets();
	});
}
