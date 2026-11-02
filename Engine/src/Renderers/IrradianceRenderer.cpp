#include "pch.h"
#include "IrradianceRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Rendering/Model.h"
#include "FrameGraph/FrameGraphData.h"

IrradianceRenderer::IrradianceRenderer()
{
	compute_shader = gDynamicRHI->createShader(L"shaders/ibl/irradiance.hlsl", ShaderType::COMPUTE);

	TextureDescription desc;
	desc.width = 128;
	desc.height = 128;
	desc.dimension = TextureDimension::TEXTURE_CUBE;
	desc.mip_levels = 1;
	desc.format = Format::R32G32B32A32_SFLOAT;
	desc.usage_flags = TextureUsage::UNORDERED_ACCESS;

	irradiance_texture = gDynamicRHI->createTexture(desc);
	irradiance_texture->setDebugName("IBL Irradiance Image");
}

void IrradianceRenderer::addPass(FrameGraph &fg, uint32_t samples_count)
{
	fg.addCallbackPass("IBL Irradiance Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(GFXRID(IBLIrradiance));
		builder.readTexture(GFXRID(Sky));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto irradiance = resources.getTexture(GFXRID(IBLIrradiance));

		// Very heavy computation
		gGlobalPipeline->setupComputePipeline(compute_shader);
		gGlobalPipeline->flushAndBind(cmd_list);

		struct Constants
		{
			uint32_t input_tex_id;
			uint32_t output_tex_id;
			uint32_t samples_count;
		} constants;
		constants.input_tex_id = resources.getReadTexture(GFXRID(Sky));
		constants.output_tex_id = irradiance->getUnorderedAccessView()->getBindlessIndex();
		constants.samples_count = samples_count;

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatch(irradiance->getWidth() / 32, irradiance->getHeight() / 32, 6);
		

		///irradiance.texture->createMipmaps(command_buffer); // TODO: mipmaps
	});
}
