#include "pch.h"
#include "IrradianceRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Model.h"
#include "FrameGraph/FrameGraphData.h"

IrradianceRenderer::IrradianceRenderer(): RendererBase()
{
	compute_shader = Shader::create("shaders/ibl/irradiance.comp", Shader::COMPUTE_SHADER);

	TextureDescription desc;
	desc.width = 128;
	desc.height = 128;
	desc.is_cube = true;
	desc.mip_levels = 5;
	desc.image_format = VK_FORMAT_R32G32B32A32_SFLOAT;
	desc.usage_flags = TEXTURE_USAGE_TRANSFER_SRC | TEXTURE_USAGE_TRANSFER_DST | TEXTURE_USAGE_STORAGE;

	irradiance_texture = Texture::create(desc);
	irradiance_texture->fill();
	irradiance_texture->setDebugName("IBL Irradiance Image");
	BindlessResources::addTexture(irradiance_texture.get());
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
	[=](const IBLData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		FrameGraphTexture &irradiance = resources.getResource<FrameGraphTexture>(data.irradiance);
		FrameGraphTexture &sky = resources.getResource<FrameGraphTexture>(sky_data.sky);

		// Very heavy computation
		auto &p = VkWrapper::global_pipeline;
		p->reset();

		p->setIsComputePipeline(true);
		p->setComputeShader(compute_shader);

		p->flush();
		p->bind(command_buffer);

		// Uniforms
		Renderer::setShadersTexture(p->getCurrentShaders(), 1, irradiance.texture, -1, -1, true);
		Renderer::setShadersTexture(p->getCurrentShaders(), 2, sky.texture);
		Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);

		vkCmdDispatch(command_buffer.get_buffer(), irradiance.texture->getWidth() / 32, irradiance.texture->getHeight() / 32, 6);

		p->unbind(command_buffer);

		irradiance.texture->generateMipmaps(command_buffer);
	});
}
