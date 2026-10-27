#include "pch.h"
#include "PostProcessingRenderer.h"
#include "RHI/BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Core/Variables.h"

void PostProcessingRenderer::addPasses(FrameGraph &fg)
{
	last_output = GFXRID(FinalNoPostTexture);
	if (Renderer::isUpscalerActive())
		last_output = GFXRID(UpscaledColor);

	addTonemapLutPass(fg);

	if (GFXOPTIONS(path_tracing))
	{
		if (GFXOPTIONS(bloom).enabled)
			addBloomPass(fg);

		addFilmPass(fg);
	} else
	{
		if (GFXOPTIONS(ssr).enabled && !Renderer::isUpscalerActive())
			last_output = GFXRID(SSR);

		if (GFXOPTIONS(bloom).enabled)
			addBloomPass(fg);

		addFilmPass(fg);

		if (Renderer::isFXAAEnabled())
			addFxaaPass(fg);
	}
}

void PostProcessingRenderer::addTonemapLutPass(FrameGraph &fg)
{
	const uint32_t lut_size = 32;

	fg.addCallbackPass("Tonemap LUT",
	[=](RenderPassBuilder &builder)
	{
		TextureDescription lut_desc;
		lut_desc.width = lut_size;
		lut_desc.height = lut_size;
		lut_desc.depth = lut_size;
		lut_desc.format = FORMAT_R16G16B16A16_SFLOAT;
		builder.createTexture(GFXRID(TonemapLUT), lut_desc);

		builder.writeUAVTexture(GFXRID(TonemapLUT));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		struct
		{
			uint32_t output_tex_id;
			int tonemapper_mode;
			alignas(16) UchimuraSettings uchimura;
		} constants;
		constants.output_tex_id = resources.getReadWriteTexture(GFXRID(TonemapLUT));
		constants.tonemapper_mode = GFXOPTIONS(film).tonemapper;
		constants.uchimura = GFXOPTIONS(film).uchimura;

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/build_lut.hlsl", COMPUTE_SHADER));
		gGlobalPipeline->flushAndBind(cmd_list);

		gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

		cmd_list->dispatch(lut_size / 4, lut_size / 4, lut_size / 4);
	});
}

void PostProcessingRenderer::addBloomPass(FrameGraph &fg)
{
	GraphicsResourceName input = last_output;

	fg.addCallbackPass("Bloom",
	[input](RenderPassBuilder &builder)
	{
		TextureDescription input_desc = builder.getTextureDescription(input);

		TextureDescription bloom_desc;
		bloom_desc.format = FORMAT_R11G11B10_UFLOAT;
		bloom_desc.width = input_desc.width / 2;
		bloom_desc.height = input_desc.height / 2;
		bloom_desc.mip_levels = std::min(GFXOPTIONS(bloom).mip_count, (uint32_t)std::log2(std::min(bloom_desc.width, bloom_desc.height)) + 1);
		builder.createTexture(GFXRID(Bloom), bloom_desc);

		builder.readTexture(input);
		builder.writeUAVTexture(GFXRID(Bloom));
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		RHITexture *bloom = resources.getTexture(GFXRID(Bloom));
		const BloomSettings &settings = GFXOPTIONS(bloom);

		auto dispatch = [&](uint32_t input_tex_id, glm::ivec2 input_size, int output_mip)
		{
			struct
			{
				uint32_t input_tex_id;
				uint32_t output_tex_id;
				glm::ivec2 output_size;
				glm::vec2 input_texel_size;
				float scatter;
				float threshold;
			} constants;
			constants.input_tex_id = input_tex_id;
			constants.output_tex_id = bloom->getUnorderedAccessView(output_mip)->getBindlessIndex();
			constants.output_size = bloom->getSize(output_mip);
			constants.input_texel_size = 1.0f / glm::vec2(input_size);
			constants.scatter = settings.scatter;
			constants.threshold = settings.threshold;

			gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

			cmd_list->dispatch(Math::divideRoundUp(constants.output_size.x, 8), Math::divideRoundUp(constants.output_size.y, 8), 1);
			bloom->transitLayout(cmd_list, TEXTURE_LAYOUT_UAV);
		};

		// Downsample
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/bloom.hlsl", COMPUTE_SHADER, "CSFirstDownsample"));
		gGlobalPipeline->flushAndBind(cmd_list);

		dispatch(resources.getReadTexture(input), resources.getTexture(input)->getSize(), 0);

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/bloom.hlsl", COMPUTE_SHADER, "CSDownsample"));
		gGlobalPipeline->flushAndBind(cmd_list);

		for (int mip = 1; mip < bloom->getMipLevels(); mip++)
			dispatch(bloom->getShaderResourceView(mip - 1)->getBindlessIndex(), bloom->getSize(mip - 1), mip);

		// Upsample
		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/bloom.hlsl", COMPUTE_SHADER, "CSUpsample"));
		gGlobalPipeline->flushAndBind(cmd_list);

		for (int mip = bloom->getMipLevels() - 2; mip >= 0; mip--)
			dispatch(bloom->getShaderResourceView(mip + 1)->getBindlessIndex(), bloom->getSize(mip + 1), mip);
	});
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
			TextureDescription output_desc = builder.getTextureDescription(last_output);
			builder.createTexture(GFXRID(FilmPassOutput), output_desc.width, output_desc.height, output_desc.format);
		}
		builder.writeTexture(output);
		builder.readTexture(last_output);
		builder.readTexture(GFXRID(TonemapLUT));
		if (GFXOPTIONS(bloom).enabled)
			builder.readTexture(GFXRID(Bloom));

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
		film_ubo.tonemap_lut_tex_id = resources.getReadTexture(GFXRID(TonemapLUT));
		film_ubo.use_vignette = film.vignette ? 1.0f : 0.0f;
		film_ubo.vignette_radius = film.vignette_radius;
		film_ubo.vignette_smoothness = film.vignette_smoothness;

		const BloomSettings &bloom = GFXOPTIONS(bloom);
		film_ubo.bloom_tex_id = bloom.enabled ? resources.getReadTexture(GFXRID(Bloom)) : 0;
		film_ubo.bloom_intensity = bloom.enabled ? bloom.intensity : 0.0f;

		gDynamicRHI->setConstantBufferData(0, &film_ubo, sizeof(FilmUBO));

		cmd_list->drawInstanced(6, 1, 0, 0);

		cmd_list->resetRenderTargets();
	});

	last_output = GFXRID(FilmPassOutput);
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
