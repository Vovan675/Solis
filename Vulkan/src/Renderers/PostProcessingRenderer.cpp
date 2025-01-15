#include "pch.h"
#include "imgui.h"
#include "PostProcessingRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

PostProcessingRenderer::PostProcessingRenderer()
{
}

PostProcessingRenderer::~PostProcessingRenderer()
{
}

void PostProcessingRenderer::addPasses(FrameGraph &fg)
{
	auto &default_data = fg.getBlackboard().get<DefaultResourcesData>();

	default_data = fg.addCallbackPass<DefaultResourcesData>("Film Pass",
	[&](RenderPassBuilder &builder, DefaultResourcesData &data)
	{
		// Setup
		data = default_data;
		data.final = builder.write(default_data.final);

		data.final_no_post = builder.read(default_data.final_no_post);
	},
	[=](const DefaultResourcesData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		// Render
		auto &final_no_post = resources.getResource<FrameGraphTexture>(data.final_no_post);
		auto &final = resources.getResource<FrameGraphTexture>(data.final);

		VkWrapper::cmdBeginRendering(command_buffer, {final.texture}, nullptr);

		ubo.composite_final_tex_id = resources.getResource<FrameGraphTexture>(default_data.final_no_post).getBindlessId();

		auto &p = VkWrapper::global_pipeline;
		p->bindScreenQuadPipeline(command_buffer, Shader::create("shaders/film.frag", Shader::FRAGMENT_SHADER));

		// Uniforms
		Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(UBO));
		Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

		vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

		p->unbind(command_buffer);

		VkWrapper::cmdEndRendering(command_buffer);
	});
}

void PostProcessingRenderer::renderImgui()
{
	bool use_vignette_bool = ubo.use_vignette > 0.5f;
	if (ImGui::Checkbox("Vignette", &use_vignette_bool))
	{
		ubo.use_vignette = use_vignette_bool ? 1.0f : 0.0f;
	}

	if (ubo.use_vignette)
	{
		ImGui::SliderFloat("Vignette Radius", &ubo.vignette_radius, 0.1f, 1.0f);
		ImGui::SliderFloat("Vignette Smoothness", &ubo.vignette_smoothness, 0.1f, 1.0f);
	}
}
