#include "pch.h"
#include "SSRRenderer.h"
#include "PostProcessingRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

void SSRRenderer::addPasses(FrameGraph &fg)
{
	auto &ssr_data = fg.getBlackboard().add<SSRData>();
	auto &gbuffer_data = fg.getBlackboard().get<GBufferData>();
	auto &lighting_data = fg.getBlackboard().get<DeferredLightingData>();

	struct SSRInternalData
	{
		FrameGraphResource ssr_direct_lighting;
	};
	auto &ssr_internal_data = fg.getBlackboard().add<SSRInternalData>();
	ssr_internal_data = fg.addCallbackPass<SSRInternalData>("SSR Direct Lighting Pass",
	[&](RenderPassBuilder &builder, SSRInternalData &data)
	{
		// Setup
		FrameGraphTexture::Description desc;
		desc.width = Renderer::getViewportSize().x;
		desc.height = Renderer::getViewportSize().y;
		desc.image_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
		desc.debug_name = "SSR Direct Lighting Image";

		data.ssr_direct_lighting = builder.createResource<FrameGraphTexture>(desc.debug_name, desc);
		data.ssr_direct_lighting = builder.write(data.ssr_direct_lighting);

		builder.read(lighting_data.diffuse_light);
		builder.read(lighting_data.specular_light);
		builder.read(gbuffer_data.albedo);
	},
	[=](const SSRInternalData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		// Render
		auto &ssr_direct_lighting = resources.getResource<FrameGraphTexture>(data.ssr_direct_lighting);

		struct UBO
		{
			uint32_t lighting_diffuse_tex_id = 0;
			uint32_t lighting_specular_tex_id = 0;
			uint32_t albedo_tex_id = 0;
		} ubo;

		ubo.lighting_diffuse_tex_id = resources.getResource<FrameGraphTexture>(lighting_data.diffuse_light).getBindlessId();
		ubo.lighting_specular_tex_id = resources.getResource<FrameGraphTexture>(lighting_data.specular_light).getBindlessId();
		ubo.albedo_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.albedo).getBindlessId();

		VkWrapper::cmdBeginRendering(command_buffer, {ssr_direct_lighting.texture}, nullptr);

		auto &p = VkWrapper::global_pipeline;
		p->bindScreenQuadPipeline(command_buffer, Shader::create("shaders/ssr_direct_light.frag", Shader::FRAGMENT_SHADER));

		// Uniforms
		Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(UBO));
		Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

		// Render quad
		vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

		p->unbind(command_buffer);
		VkWrapper::cmdEndRendering(command_buffer);
	});

	ssr_data = fg.addCallbackPass<SSRData>("SSR Pass",
	[&](RenderPassBuilder &builder, SSRData &data)
	{
		// Setup
		FrameGraphTexture::Description desc;
		desc.width = Renderer::getViewportSize().x;
		desc.height = Renderer::getViewportSize().y;
		desc.image_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
		desc.debug_name = "SSR Image";

		data.ssr = builder.createResource<FrameGraphTexture>(desc.debug_name, desc);
		data.ssr = builder.write(data.ssr);

		builder.read(ssr_internal_data.ssr_direct_lighting); // TODO: replace with albedo with applied direct lighting?
		builder.read(gbuffer_data.normal);
		builder.read(gbuffer_data.depth);
	},
	[=](const SSRData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		// Render
		auto &ssr = resources.getResource<FrameGraphTexture>(data.ssr);

		struct UBO
		{
			uint32_t color_tex_id = 0;
			uint32_t normal_tex_id = 0;
			uint32_t depth_tex_id = 0;
		} ubo;
		ubo.color_tex_id = resources.getResource<FrameGraphTexture>(ssr_internal_data.ssr_direct_lighting).getBindlessId();
		ubo.normal_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.normal).getBindlessId();
		ubo.depth_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.depth).getBindlessId();

		if (Renderer::screen_resources[RENDER_TARGET_FINAL_NO_POST])
		{
			//Renderer::screen_resources[RENDER_TARGET_FINAL_NO_POST]->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
			//ubo.color_tex_id = BindlessResources::getTextureIndex(Renderer::screen_resources[RENDER_TARGET_FINAL_NO_POST].get());
		}

		VkWrapper::cmdBeginRendering(command_buffer, {ssr.texture}, nullptr);

		auto &p = VkWrapper::global_pipeline;
		p->bindScreenQuadPipeline(command_buffer, Shader::create("shaders/ssr.frag", Shader::FRAGMENT_SHADER));

		// Uniforms
		Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(UBO));
		Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

		// Render quad
		vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

		p->unbind(command_buffer);
		VkWrapper::cmdEndRendering(command_buffer);
	});
}
