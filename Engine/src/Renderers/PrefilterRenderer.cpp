#include "pch.h"
#include "PrefilterRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Rendering/Model.h"
#include "FrameGraph/FrameGraphData.h"

PrefilterRenderer::PrefilterRenderer(): RendererBase()
{
	compute_shader = gDynamicRHI->createShader(L"shaders/ibl/prefilter.hlsl", COMPUTE_SHADER);

	TextureDescription desc;
	desc.width = 512;
	desc.height = 512;
	desc.is_cube = true;
	desc.mip_levels = 5;
	desc.format = FORMAT_R32G32B32A32_SFLOAT;
	desc.usage_flags = TEXTURE_USAGE_STORAGE;

	prefilter_texture = gDynamicRHI->createTexture(desc);
	prefilter_texture->fill();
	prefilter_texture->setDebugName("IBL Prefilter Image");
}

void PrefilterRenderer::addPass(FrameGraph &fg, uint32_t samples_count)
{
	fg.addCallbackPass("IBL Prefilter Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(GFXRID(IBLPrefilter));
		builder.readTexture(GFXRID(Sky));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto prefilter = resources.getTexture(GFXRID(IBLPrefilter));
		auto sky = resources.getTexture(GFXRID(Sky));

		const int MIP_COUNT = 5;

		constants_frag.input_tex_id = resources.getReadTexture(GFXRID(Sky));
		constants_frag.mip_count = MIP_COUNT;
		constants_frag.samples_count = samples_count;

		for (int mip = 0; mip < MIP_COUNT; mip++)
		{
			gGlobalPipeline->setupComputePipeline(compute_shader);
			gGlobalPipeline->flushAndBind(cmd_list);

			float roughness = (float)mip / (float)(5 - 1);
			constants_frag.roughness = roughness;
			constants_frag.output_tex_id = prefilter->getUnorderedAccessView(mip)->getBindlessIndex();

			gDynamicRHI->setConstantBufferData(0, &constants_frag, sizeof(PushConstantFrag));

			cmd_list->dispatch(prefilter->getWidth(mip) / 32, prefilter->getHeight(mip) / 32, 6);

		}
	});
	
}
