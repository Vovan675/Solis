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
	desc.image_format = VK_FORMAT_R16G16_SFLOAT;
	desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
	desc.sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	brdf_lut_texture = Texture::create(desc);
	brdf_lut_texture->fill();
	brdf_lut_texture->setDebugName("IBL BRDF LUT Image");
	BindlessResources::addTexture(brdf_lut_texture.get());
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
