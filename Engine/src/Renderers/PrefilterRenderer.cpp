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
	gDynamicRHI->getBindlessResources()->addTexture(prefilter_texture);
}

void PrefilterRenderer::addPass(FrameGraph &fg, uint32_t samples_count)
{
	fg.addCallbackPass<EmptyData>("IBL Prefilter Pass",
	[&](RenderPassBuilder &builder, EmptyData &data)
	{
		builder.writeTexture(GFXRID(IBLPrefilter), TEXTURE_RESOURCE_ACCESS_GENERAL);
		builder.readTexture(GFXRID(Sky));
	},
	[=](const EmptyData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		FrameGraphTexture &prefilter = resources.getResource<FrameGraphTexture>(GFXRID(IBLPrefilter));
		FrameGraphTexture &sky = resources.getResource<FrameGraphTexture>(GFXRID(Sky));

		const int MIP_COUNT = 5;

		constants_frag.input_tex_id = sky.getBindlessId();
		constants_frag.mip_count = MIP_COUNT;
		constants_frag.samples_count = samples_count;

		for (int mip = 0; mip < MIP_COUNT; mip++)
		{
			auto &p = gGlobalPipeline;
			p->reset();

			p->setIsComputePipeline(true);
			p->setComputeShader(compute_shader);

			p->flush();
			p->bind(cmd_list);

			float roughness = (float)mip / (float)(5 - 1);
			constants_frag.roughness = roughness;

			// Uniforms
			gDynamicRHI->setUAVTexture(1, prefilter.texture, mip);
			gDynamicRHI->setConstantBufferData(0, &constants_frag, sizeof(PushConstantFrag));

			cmd_list->dispatch(prefilter.texture->getWidth() / 32, prefilter.texture->getHeight() / 32, 6);

			p->unbind(cmd_list);
		}
	});
	
}
