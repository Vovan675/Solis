#include "pch.h"
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

	if (GFXOPTIONS(path_tracing))
	{
		addFilmPass(fg);
	} else
	{
		if (GFXOPTIONS(ssr).enabled && !Renderer::isUpscalerActive())
			last_output = GFXRID(SSR);

		addFilmPass(fg);

		if (Renderer::isFXAAEnabled())
			addFxaaPass(fg);
	}
}

void PostProcessingRenderer::addFilmPass(FrameGraph &fg)
{
	GraphicsResourceName output;
	if (GFXOPTIONS(path_tracing))
	{
		output = GFXRID(FinalTexture);
	} else
	{
		output = GFXRID(FilmPassOutput);
		if (!Renderer::isFXAAEnabled())
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

		const FilmSettings &film = GFXOPTIONS(film);
		film_ubo.composite_final_tex_id = resources.getReadTexture(data.input);
		film_ubo.exposure = film.getExposure();
		film_ubo.tonemapper_mode = film.tonemapper;
		film_ubo.use_vignette = film.vignette ? 1.0f : 0.0f;
		film_ubo.vignette_radius = film.vignette_radius;
		film_ubo.vignette_smoothness = film.vignette_smoothness;

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
