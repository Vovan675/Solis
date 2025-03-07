#include "pch.h"
#include "IrradianceRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Model.h"
#include "FrameGraph/FrameGraphData.h"

IrradianceRenderer::IrradianceRenderer(): RendererBase()
{
	compute_shader = gDynamicRHI->createShader(L"shaders/ibl/irradiance.hlsl", COMPUTE_SHADER);

	TextureDescription desc;
	desc.width = 128;
	desc.height = 128;
	desc.is_cube = true;
	desc.mip_levels = 5;
	desc.format = FORMAT_R32G32B32A32_SFLOAT;
	desc.usage_flags = TEXTURE_USAGE_TRANSFER_SRC | TEXTURE_USAGE_TRANSFER_DST | TEXTURE_USAGE_STORAGE;

	irradiance_texture = gDynamicRHI->createTexture(desc);
	irradiance_texture->fill();
	irradiance_texture->setDebugName("IBL Irradiance Image");
	gDynamicRHI->getBindlessResources()->addTexture(irradiance_texture);
}

void IrradianceRenderer::addPass(FrameGraph &fg)
{
	SkyData &sky_data = fg.getBlackboard().get<SkyData>();
	IBLData &ibl_data = fg.getBlackboard().get<IBLData>();

	ibl_data = fg.addCallbackPass<IBLData>("IBL Irradiance Pass",
	[&](RenderPassBuilder &builder, IBLData &data)
	{
		data.prefilter = ibl_data.prefilter;
		data.irradiance = builder.write(ibl_data.irradiance, TEXTURE_RESOURCE_ACCESS_GENERAL);

		builder.read(sky_data.sky);
	},
	[=](const IBLData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		FrameGraphTexture &irradiance = resources.getResource<FrameGraphTexture>(data.irradiance);
		FrameGraphTexture &sky = resources.getResource<FrameGraphTexture>(sky_data.sky);

		// Very heavy computation
		auto &p = gGlobalPipeline;
		p->reset();

		p->setIsComputePipeline(true);
		p->setComputeShader(compute_shader);

		p->flush();
		p->bind(cmd_list);

		// Uniforms
		gDynamicRHI->setUAVTexture(1, irradiance.texture);
		gDynamicRHI->setTexture(2, sky.texture);

		cmd_list->dispatch(irradiance.texture->getWidth() / 32, irradiance.texture->getHeight() / 32, 6);
		
		p->unbind(cmd_list);

		///irradiance.texture->generateMipmaps(command_buffer); // TODO: mipmaps
	});
}
