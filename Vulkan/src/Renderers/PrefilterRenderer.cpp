#include "pch.h"
#include "PrefilterRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Model.h"
#include "FrameGraph/FrameGraphData.h"

PrefilterRenderer::PrefilterRenderer(): RendererBase()
{
	compute_shader = Shader::create("shaders/ibl/prefilter.comp", Shader::COMPUTE_SHADER);

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

	prefilter_texture = create_screen_texture(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
										   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, "IBL Prefilter Image");
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
	[=](const IBLData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		FrameGraphTexture &prefilter = resources.getResource<FrameGraphTexture>(data.prefilter);
		FrameGraphTexture &sky = resources.getResource<FrameGraphTexture>(sky_data.sky);

		for (int mip = 0; mip < 5; mip++)
		{
			auto &p = VkWrapper::global_pipeline;
			p->reset();

			p->setIsComputePipeline(true);
			p->setComputeShader(compute_shader);

			p->flush();
			p->bind(command_buffer);

			float roughness = (float)mip / (float)(5 - 1);
			constants_frag.roughness = roughness;

			// Uniforms
			Renderer::setShadersTexture(p->getCurrentShaders(), 1, prefilter.texture, mip, -1, true);
			Renderer::setShadersTexture(p->getCurrentShaders(), 2, sky.texture);
			Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);

			vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantFrag), &constants_frag);

			vkCmdDispatch(command_buffer.get_buffer(), prefilter.texture->getWidth() / 32, prefilter.texture->getHeight() / 32, 6);
			p->unbind(command_buffer);
		}
	});
	
}
