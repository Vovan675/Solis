#include "pch.h"
#include "PrefilterRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Model.h"
#include "FrameGraph/FrameGraphData.h"

PrefilterRenderer::PrefilterRenderer(): RendererBase()
{
	compute_shader = gDynamicRHI->createShader(L"shaders/ibl/prefilter.hlsl", COMPUTE_SHADER);

	TextureDescription desc;
	desc.width = 128;
	desc.height = 128;
	desc.is_cube = true;
	desc.mip_levels = 5;
	desc.format = FORMAT_R32G32B32A32_SFLOAT;
	desc.usage_flags = TEXTURE_USAGE_STORAGE;

	prefilter_texture = gDynamicRHI->createTexture(desc);
	prefilter_texture->fill();
	prefilter_texture->setDebugName("IBL Prefilter Image");
	gDynamicRHI->getBindlessResources()->addTexture(prefilter_texture.get());
}

void PrefilterRenderer::addPass(FrameGraph &fg)
{
	SkyData &sky_data = fg.getBlackboard().get<SkyData>();
	IBLData &ibl_data = fg.getBlackboard().get<IBLData>();

	ibl_data = fg.addCallbackPass<IBLData>("IBL Prefilter Pass",
	[&](RenderPassBuilder &builder, IBLData &data)
	{
		data.prefilter = builder.write(ibl_data.prefilter, TEXTURE_RESOURCE_ACCESS_GENERAL);
		data.irradiance = ibl_data.irradiance;

		builder.read(sky_data.sky);
	},
	[=](const IBLData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		FrameGraphTexture &prefilter = resources.getResource<FrameGraphTexture>(data.prefilter);
		FrameGraphTexture &sky = resources.getResource<FrameGraphTexture>(sky_data.sky);

		for (int mip = 0; mip < 5; mip++)
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
			gDynamicRHI->setUAVTexture(1, prefilter.texture, mip); // mip, uav
			gDynamicRHI->setTexture(2, sky.texture);
			gDynamicRHI->setConstantBufferData(0, &constants_frag, sizeof(PushConstantFrag));

			cmd_list->dispatch(prefilter.texture->getWidth() / 32, prefilter.texture->getHeight() / 32, 6);

			p->unbind(cmd_list);
		}
	});
	
}
