#include "pch.h"
#include "IrradianceRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Rendering/Model.h"
#include "FrameGraph/FrameGraphData.h"

IrradianceRenderer::IrradianceRenderer(): RendererBase()
{
	compute_shader = gDynamicRHI->createShader(L"shaders/ibl/irradiance.hlsl", COMPUTE_SHADER);

	TextureDescription desc;
	desc.width = 128;
	desc.height = 128;
	desc.is_cube = true;
	desc.mip_levels = 1;
	desc.format = FORMAT_R32G32B32A32_SFLOAT;
	desc.usage_flags = TEXTURE_USAGE_TRANSFER_SRC | TEXTURE_USAGE_TRANSFER_DST | TEXTURE_USAGE_STORAGE;

	irradiance_texture = gDynamicRHI->createTexture(desc);
	irradiance_texture->fill();
	irradiance_texture->setDebugName("IBL Irradiance Image");
	gDynamicRHI->getBindlessResources()->addTexture(irradiance_texture);
}

void IrradianceRenderer::addPass(FrameGraph &fg, uint32_t samples_count)
{
	fg.addCallbackPass<EmptyData>("IBL Irradiance Pass",
	[&](RenderPassBuilder &builder, EmptyData &data)
	{
		builder.writeTexture(GFXRID(IBLIrradiance), TEXTURE_RESOURCE_ACCESS_GENERAL);
		builder.readTexture(GFXRID(Sky));
	},
	[=](const EmptyData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		FrameGraphTexture &irradiance = resources.getResource<FrameGraphTexture>(GFXRID(IBLIrradiance));
		FrameGraphTexture &sky = resources.getResource<FrameGraphTexture>(GFXRID(Sky));

		// Very heavy computation
		auto &p = gGlobalPipeline;
		p->reset();

		p->setIsComputePipeline(true);
		p->setComputeShader(compute_shader);

		p->flush();
		p->bind(cmd_list);

		struct Constants
		{
			uint32_t input_tex_id;
			uint32_t samples_count;
		} constants;
		constants.input_tex_id = sky.getBindlessId();
		constants.samples_count = samples_count;

		gDynamicRHI->setUAVTexture(1, irradiance.texture);
		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatch(irradiance.texture->getWidth() / 32, irradiance.texture->getHeight() / 32, 6);
		
		p->unbind(cmd_list);

		///irradiance.texture->createMipmaps(command_buffer); // TODO: mipmaps
	});
}
