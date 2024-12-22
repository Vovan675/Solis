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
	auto &lighting_data = fg.getBlackboard().get<DefferedLightingData>();
	auto &ssao_data = fg.getBlackboard().get<SSAOData>();
	auto &ibl_data = fg.getBlackboard().get<IBLData>();
	auto &lut_data = fg.getBlackboard().get<LutData>();

	auto &composite_data = fg.getBlackboard().get<CompositeData>();

	composite_data = fg.addCallbackPass<CompositeData>("Deffered Composite Pass",
	[&](RenderPassBuilder &builder, CompositeData &data)
	{
		// Setup
		data.composite = builder.write(composite_data.composite);

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
	},
	[=](const CompositeData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		// Render
		auto &composite = resources.getResource<FrameGraphTexture>(data.composite);
		auto &ibl_irradiance = resources.getResource<FrameGraphTexture>(ibl_data.irradiance);
		auto &ibl_prefilter = resources.getResource<FrameGraphTexture>(ibl_data.prefilter);

		VkWrapper::cmdBeginRendering(command_buffer, {composite.texture}, nullptr, -1, 0, false);

		ubo.lighting_diffuse_tex_id = resources.getResource<FrameGraphTexture>(lighting_data.diffuse_light).getBindlessId();
		ubo.lighting_specular_tex_id = resources.getResource<FrameGraphTexture>(lighting_data.specular_light).getBindlessId();
		ubo.albedo_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.albedo).getBindlessId();
		ubo.normal_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.normal).getBindlessId();
		ubo.depth_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.depth).getBindlessId();
		ubo.shading_tex_id = resources.getResource<FrameGraphTexture>(gbuffer_data.shading).getBindlessId();
		ubo.brdf_lut_tex_id = resources.getResource<FrameGraphTexture>(lut_data.brdf_lut).getBindlessId();
		ubo.ssao_tex_id = resources.getResource<FrameGraphTexture>(ssao_data.ssao_blurred).getBindlessId();

		auto &p = VkWrapper::global_pipeline;
		p->bindScreenQuadPipeline(command_buffer, Shader::create("shaders/deffered_composite.frag", Shader::FRAGMENT_SHADER));

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
}

void DefferedCompositeRenderer::renderImgui()
{
}
