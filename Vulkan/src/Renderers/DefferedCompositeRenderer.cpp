#include "pch.h"
#include "DefferedCompositeRenderer.h"
#include "imgui.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

DefferedCompositeRenderer::DefferedCompositeRenderer()
{
}

DefferedCompositeRenderer::~DefferedCompositeRenderer()
{
}

void DefferedCompositeRenderer::addPasses(FrameGraph &fg)
{
	auto &gbuffer_data = fg.getBlackboard().get<GBufferData>();
	auto &lighting_data = fg.getBlackboard().get<DeferredLightingData>();
	auto &ssao_data = fg.getBlackboard().get<SSAOData>();
	auto *ssr_data = fg.getBlackboard().tryGet<SSRData>();
	auto &ibl_data = fg.getBlackboard().get<IBLData>();
	auto &lut_data = fg.getBlackboard().get<LutData>();

	auto &composite_data = fg.getBlackboard().get<CompositeData>();
	auto &default_data = fg.getBlackboard().get<DefaultResourcesData>();

	composite_data = fg.addCallbackPass<CompositeData>("Composite Indirect Pass",
	[&](RenderPassBuilder &builder, CompositeData &data)
	{
		// Setup
		FrameGraphTexture::Description desc;
		desc.width = Renderer::getViewportSize().x;
		desc.height = Renderer::getViewportSize().y;
		desc.image_format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		desc.usage_flags = TEXTURE_USAGE_ATTACHMENT;
		desc.sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		data = composite_data;

		desc.debug_name = "Indirect Ambient Image";
		data.composite_indirect_ambient = builder.createResource<FrameGraphTexture>("Indirect Ambient Image", desc);
		desc.debug_name = "Indirect Specular Image";
		data.composite_indirect_specular = builder.createResource<FrameGraphTexture>("Indirect Specular Image", desc);

		data.composite_indirect_ambient = builder.write(data.composite_indirect_ambient);
		data.composite_indirect_specular = builder.write(data.composite_indirect_specular);

		builder.read(lighting_data.diffuse_light);
		builder.read(lighting_data.specular_light);
		builder.read(gbuffer_data.albedo);
		builder.read(gbuffer_data.normal);
		builder.read(gbuffer_data.depth);
		builder.read(gbuffer_data.shading);
		builder.read(lut_data.brdf_lut);
		builder.read(ibl_data.irradiance);
		builder.read(ibl_data.prefilter);
		builder.read(ssao_data.ssao_blurred);

		if (ssr_data)
			builder.read(ssr_data->ssr);
	},
	[=](const CompositeData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		// Render
		auto &indirect_ambient = resources.getResource<FrameGraphTexture>(data.composite_indirect_ambient);
		auto &indirect_specular = resources.getResource<FrameGraphTexture>(data.composite_indirect_specular);
		auto &ibl_irradiance = resources.getResource<FrameGraphTexture>(ibl_data.irradiance);
		auto &ibl_prefilter = resources.getResource<FrameGraphTexture>(ibl_data.prefilter);

		VkWrapper::cmdBeginRendering(command_buffer, {indirect_ambient.texture, indirect_specular.texture}, nullptr, -1, 0, false);

		struct UBO
		{
			uint32_t lighting_diffuse_tex_id = 0;
			uint32_t lighting_specular_tex_id = 0;
			uint32_t albedo_tex_id = 0;
			uint32_t normal_tex_id = 0;
			uint32_t depth_tex_id = 0;
			uint32_t shading_tex_id = 0;
			uint32_t brdf_lut_tex_id = 0;
			uint32_t ssao_tex_id = 0;
			uint32_t ssr_tex_id = 0;
		} ubo;

		ubo.lighting_diffuse_tex_id = resources.getResource<FrameGraphTexture>(lighting_data.diffuse_light).getBindlessId();
		ubo.lighting_specular_tex_id = resources.getResource<FrameGraphTexture>(lighting_data.specular_light).getBindlessId();
		ubo.albedo_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.albedo).getBindlessId();
		ubo.normal_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.normal).getBindlessId();
		ubo.depth_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.depth).getBindlessId();
		ubo.shading_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.shading).getBindlessId();
		ubo.brdf_lut_tex_id = resources.getResource<FrameGraphTexture>(lut_data.brdf_lut).getBindlessId();
		ubo.ssao_tex_id = resources.getResource<FrameGraphTexture>(ssao_data.ssao_blurred).getBindlessId();

		if (ssr_data)
			ubo.ssr_tex_id = resources.getResource<FrameGraphTexture>(ssr_data->ssr).getBindlessId();

		auto shader = Shader::create("shaders/composite_indirect.frag", Shader::FRAGMENT_SHADER, {{"SSR", ssr_data ? "1" : "0"}});
		
		auto &p = VkWrapper::global_pipeline;
		p->bindScreenQuadPipeline(command_buffer, shader);

		Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(UBO));
		Renderer::setShadersTexture(p->getCurrentShaders(), 1, ibl_irradiance.texture);
		Renderer::setShadersTexture(p->getCurrentShaders(), 2, ibl_prefilter.texture);

		// Uniforms
		Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

		// Render quad
		vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

		p->unbind(command_buffer);

		VkWrapper::cmdEndRendering(command_buffer);
	});

	default_data = fg.addCallbackPass<DefaultResourcesData>("Deffered Composite Pass",
	[&](RenderPassBuilder &builder, DefaultResourcesData &data)
	{
		// Setup
		data = default_data;
		data.final_no_post = builder.write(default_data.final_no_post);

		builder.read(composite_data.composite_indirect_ambient);
		builder.read(composite_data.composite_indirect_specular);
		builder.read(lighting_data.diffuse_light);
		builder.read(lighting_data.specular_light);
		builder.read(gbuffer_data.albedo);
		builder.read(gbuffer_data.depth);
	},
	[=](const DefaultResourcesData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		// Render
		auto &composite = resources.getResource<FrameGraphTexture>(data.final_no_post);

		VkWrapper::cmdBeginRendering(command_buffer, {composite.texture}, nullptr, -1, 0, false);

		struct UBO
		{
			uint32_t lighting_diffuse_tex_id = 0;
			uint32_t lighting_specular_tex_id = 0;
			uint32_t indirect_ambient_tex_id = 0;
			uint32_t indirect_specular_tex_id = 0;
			uint32_t albedo_tex_id = 0;
			uint32_t depth_tex_id = 0;
		} ubo;

		ubo.lighting_diffuse_tex_id = resources.getResource<FrameGraphTexture>(lighting_data.diffuse_light).getBindlessId();
		ubo.lighting_specular_tex_id = resources.getResource<FrameGraphTexture>(lighting_data.specular_light).getBindlessId();
		ubo.indirect_ambient_tex_id = resources.getResource<FrameGraphTexture>(composite_data.composite_indirect_ambient).getBindlessId();
		ubo.indirect_specular_tex_id = resources.getResource<FrameGraphTexture>(composite_data.composite_indirect_specular).getBindlessId();
		ubo.albedo_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.albedo).getBindlessId();
		ubo.depth_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.depth).getBindlessId();

		auto &p = VkWrapper::global_pipeline;
		p->bindScreenQuadPipeline(command_buffer, Shader::create("shaders/deffered_composite.frag", Shader::FRAGMENT_SHADER));

		Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(UBO));

		// Uniforms
		Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

		// Render quad
		vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

		p->unbind(command_buffer);

		VkWrapper::cmdEndRendering(command_buffer);
	});
}

void DefferedCompositeRenderer::renderImgui()
{
}
