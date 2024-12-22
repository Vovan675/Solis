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
	desc.imageFormat = VK_FORMAT_R16G16_SFLOAT;
	desc.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	desc.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	desc.sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	desc.is_cube = false;

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

	brdf_lut_texture = create_screen_texture(VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
										  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, "IBL BRDF LUT Image", false, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
}

void LutRenderer::addPasses(FrameGraph &fg)
{
	LutData &lut_data = fg.getBlackboard().get<LutData>();

	lut_data = fg.addCallbackPass<LutData>("BRDF LUT Pass",
	[&](RenderPassBuilder &builder, LutData &data)
	{
		data.brdf_lut = builder.write(lut_data.brdf_lut);
	},
	[=](const LutData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		FrameGraphTexture &brdf_lut = resources.getResource<FrameGraphTexture>(data.brdf_lut);

		VkWrapper::cmdBeginRendering(command_buffer, {brdf_lut.texture}, nullptr);
		auto &p = VkWrapper::global_pipeline;
		p->bindScreenQuadPipeline(command_buffer, Shader::create("shaders/ibl/brdf_lut.frag", Shader::FRAGMENT_SHADER));


		// Render quad
		vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

		p->unbind(command_buffer);
		VkWrapper::cmdEndRendering(command_buffer);
	});
}
