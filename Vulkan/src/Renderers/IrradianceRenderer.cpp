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
	desc.mipLevels = 5;

	auto create_screen_texture = [&desc](VkFormat format, VkImageAspectFlags aspect_flags, VkImageUsageFlags usage_flags, const char *name = nullptr, bool anisotropy = false, VkSamplerAddressMode sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
	{
		desc.imageFormat = format;
		desc.imageAspectFlags = aspect_flags;
		desc.imageUsageFlags = usage_flags;
		desc.sampler_address_mode = sampler_address_mode;
		desc.anisotropy = anisotropy;
		auto texture = Texture::create(desc);
		texture->fill();
		if (name)
			texture->setDebugName(name);

		BindlessResources::addTexture(texture.get());
		return texture;
	};

	irradiance_texture = create_screen_texture(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
											  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT, "IBL Irradiance Image");
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

		irradiance.texture->generate_mipmaps(command_buffer);
	});
}
