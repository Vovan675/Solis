#include "pch.h"
#include "imgui.h"
#include "PostProcessingRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Core/Variables.h"

PostProcessingRenderer::PostProcessingRenderer()
{
}

PostProcessingRenderer::~PostProcessingRenderer()
{
}

void PostProcessingRenderer::addPasses(FrameGraph &fg)
{
	addFilmPass(fg);

	if (render_fxaa)
		addFxaaPass(fg);
}

void PostProcessingRenderer::renderImgui()
{
	if (ImGui::TreeNode("Post Processing"))
	{
		bool use_vignette_bool = film_ubo.use_vignette > 0.5f;
		if (ImGui::Checkbox("Vignette", &use_vignette_bool))
		{
			film_ubo.use_vignette = use_vignette_bool ? 1.0f : 0.0f;
		}

		if (film_ubo.use_vignette)
		{
			ImGui::SliderFloat("Vignette Radius", &film_ubo.vignette_radius, 0.1f, 1.0f);
			ImGui::SliderFloat("Vignette Smoothness", &film_ubo.vignette_smoothness, 0.1f, 1.0f);
		}

		ImGui::SliderFloat("Exposure", &film_ubo.exposure, 0.1f, 4.0f);

		const char *tonemappers[] = {"Disabled", "Uncharted2", "ACES"};
		ImGui::Combo("Tonemapper", &film_ubo.tonemapper_mode, tonemappers, _countof(tonemappers));
		ImGui::TreePop();
	}
}

void PostProcessingRenderer::addFilmPass(FrameGraph &fg)
{
	auto &default_data = fg.getBlackboard().get<DefaultResourcesData>();
	auto &final_desc = fg.getDescription<FrameGraphTexture>(default_data.final);

	auto *ssr_data = fg.getBlackboard().tryGet<SSRData>();

	struct PassData
	{
		FrameGraphResource input_color;
		FrameGraphResource output;
	} pass_data;

	pass_data = fg.addCallbackPass<PassData>("Film Pass",
	[&](RenderPassBuilder &builder, PassData &data)
	{
		// Setup
		if (render_fxaa)
		{
			data.output = builder.createResource<FrameGraphTexture>("Film Pass Output", final_desc);
			data.output = builder.write(data.output);
		}
		else
		{
			data.output = builder.write(default_data.final);
		}

		data.input_color = default_data.final_no_post;
		if (ssr_data)
			data.input_color = ssr_data->ssr;

		data.input_color = builder.read(data.input_color);
	},
	[=](const PassData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		// Render
		auto &output = resources.getResource<FrameGraphTexture>(data.output);

		cmd_list->setRenderTargets({output.texture}, {}, 0, 0, true);

		// PSO
		gGlobalPipeline->reset();
		gGlobalPipeline->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/film.hlsl", FRAGMENT_SHADER));

		film_ubo.composite_final_tex_id = resources.getResource<FrameGraphTexture>(data.input_color).getBindlessId();

		gDynamicRHI->setConstantBufferData(0, &film_ubo, sizeof(FilmUBO));

		cmd_list->drawInstanced(6, 1, 0, 0);

		gGlobalPipeline->unbind(cmd_list);
		cmd_list->resetRenderTargets();
	});

	if (render_fxaa)
		current_output = pass_data.output;
	else
		default_data.final = pass_data.output;
}

void PostProcessingRenderer::addFxaaPass(FrameGraph &fg)
{
	auto &default_data = fg.getBlackboard().get<DefaultResourcesData>();

	default_data = fg.addCallbackPass<DefaultResourcesData>("FXAA Pass",
	[&](RenderPassBuilder &builder, DefaultResourcesData &data)
	{
		// Setup
		data = default_data;
		data.final = builder.write(default_data.final);

		builder.read(current_output);
	},
	[=](const DefaultResourcesData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		// Render
		auto &final = resources.getResource<FrameGraphTexture>(data.final);

		cmd_list->setRenderTargets({final.texture}, {}, 0, 0, true);

		// PSO
		gGlobalPipeline->reset();
		gGlobalPipeline->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/fxaa.hlsl", FRAGMENT_SHADER));

		struct UBO
		{
			uint32_t composite_final_tex_id = 0;
		} fxaa_ubo;
		fxaa_ubo.composite_final_tex_id = resources.getResource<FrameGraphTexture>(current_output).getBindlessId();

		gDynamicRHI->setConstantBufferData(0, &fxaa_ubo, sizeof(UBO));

		cmd_list->drawInstanced(6, 1, 0, 0);

		gGlobalPipeline->unbind(cmd_list);
		cmd_list->resetRenderTargets();
	});
}
