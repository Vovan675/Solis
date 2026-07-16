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
	last_output = GFXRID(FinalNoPostTexture);
	if (Renderer::isUpscalerActive())
		last_output = GFXRID(UpscaledColor);

	if (render_path_tracing)
	{
		addFilmPass(fg);
	} else
	{
		if (render_ssr && !Renderer::isUpscalerActive())
			last_output = GFXRID(SSR);

		addFilmPass(fg);

		if (render_fxaa)
			addFxaaPass(fg);
	}
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
	GraphicsResourceName output;
	if (render_path_tracing)
	{
		output = GFXRID(FinalTexture);
	} else
	{
		output = GFXRID(FilmPassOutput);
		if (!render_fxaa)
			output = GFXRID(FinalTexture);
	}

	struct PassData
	{
		GraphicsResourceName input;
		GraphicsResourceName output;
	};

	fg.addCallbackPass<PassData>("Film Pass",
	[&](RenderPassBuilder &builder, PassData &data)
	{
		if (!builder.isTextureCreated(output))
		{
			auto &output_desc = builder.getTextureDescription(last_output);
			builder.createTexture(GFXRID(FilmPassOutput), output_desc.width, output_desc.height, output_desc.format);
		}
		builder.writeTexture(output);
		builder.readTexture(last_output);

		data.output = output;
		data.input = last_output;
	},
	[=](const PassData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto output = resources.getTexture(data.output);

		cmd_list->setRenderTargets({output}, {}, 0, 0, true);

		// PSO
		gGlobalPipeline->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/film.hlsl", FRAGMENT_SHADER));

		film_ubo.composite_final_tex_id = resources.getReadTexture(data.input);

		gDynamicRHI->setConstantBufferData(0, &film_ubo, sizeof(FilmUBO));

		cmd_list->drawInstanced(6, 1, 0, 0);

		cmd_list->resetRenderTargets();
	});

	last_output = GFXRID(FilmPassOutput);
	last_output = GraphicsResourceName("FilmPassOutput", crc::crc32("FilmPassOutput"));
}

void PostProcessingRenderer::addFxaaPass(FrameGraph &fg)
{
	fg.addCallbackPass("FXAA Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.writeTexture(GFXRID(FinalTexture));

		builder.readTexture(last_output);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		// Render
		auto final = resources.getTexture(GFXRID(FinalTexture));

		cmd_list->setRenderTargets({final}, {}, 0, 0, true);

		// PSO
		gGlobalPipeline->bindScreenQuadPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/fxaa.hlsl", FRAGMENT_SHADER));

		struct UBO
		{
			uint32_t composite_final_tex_id = 0;
		} fxaa_ubo;
		fxaa_ubo.composite_final_tex_id = resources.getReadTexture(last_output);

		gDynamicRHI->setConstantBufferData(0, &fxaa_ubo, sizeof(UBO));

		cmd_list->drawInstanced(6, 1, 0, 0);

		cmd_list->resetRenderTargets();
	});
}
