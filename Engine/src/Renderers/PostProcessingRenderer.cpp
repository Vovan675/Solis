#include "pch.h"
#include "Editor/UI.h"
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

// Get how much nits would be WHITE on current "camera" when EV100 = 0
static float getLuminanceMax()
{
	// https://en.wikipedia.org/wiki/Film_speed#Measurements_and_calculations
	//float lens_attenuation = 0.78f; // ideal lens (pi/4)
	float lens_attenuation = 0.65f; // more typical lens (pi/4 * vignette * cosine angle etc)

	float iso_constant = 0.78f;
	float luminance_max = iso_constant / lens_attenuation;
	return luminance_max;
}

// luminance_max - maximum nits needed to fully saturate sensor
static float EV100ToLuminance(float luminance_max, float ev100)
{
	return luminance_max * pow(2.0f, ev100);
}

static float luminanceToEV100(float luminance_max, float luminance)
{
	return log2(luminance / luminance_max);
}

static float cameraToEV100(float aperture, float shutter_speed, float iso)
{
	// https://en.wikipedia.org/wiki/Exposure_value
	return log2(aperture * aperture / shutter_speed) - log2(iso / 100.0f);
}

static const float aperture_steps_per_doubling = 2.0f;
static const float shutter_steps_per_doubling = -1.0f;
static const float iso_steps_per_doubling = 1.0f;

static bool stopSlider(const char *label, float *value, float base, float steps_per_doubling, int min_step, int max_step, const char *display)
{
	int step = roundf(log2(*value / base) * steps_per_doubling);
	bool changed = UI::property(label, [&] { return ImGui::SliderInt("##value", &step, min_step, max_step, display); });
	if (changed)
		*value = base * powf(2.0f, step / steps_per_doubling);
	return changed;
}

void PostProcessingRenderer::renderImgui()
{
	ImGui::SeparatorText("Exposure");

	const char *mode_items[] = {"EV100", "Camera"};
	UI::radio("Set By", &exposure_mode, mode_items, IM_ARRAYSIZE(mode_items));

	float luminance = 1.0f / film_ubo.exposure; // because exposure is multiplier of nits, so nits is 1.0/exposure
	float ev100 = luminanceToEV100(getLuminanceMax(), luminance);

	bool is_camera_mode = exposure_mode == EXPOSURE_MODE_CAMERA;
	ImGui::BeginDisabled(is_camera_mode);
	if (UI::sliderFloat("EV100", &ev100, -6.0f, 20.0f, "%.2f EV", false, "Scene luminance that saturates the sensor. +1 EV halves the image brightness"))
		film_ubo.exposure = 1.0f / EV100ToLuminance(getLuminanceMax(), ev100);
	ImGui::EndDisabled();

	ImGui::BeginDisabled(!is_camera_mode);

	std::string aperture_text = aperture < 10.0f ? fmt::format("f/{:.1f}", aperture) : fmt::format("f/{:.0f}", aperture);
	stopSlider("Aperture", &aperture, 1.0f, aperture_steps_per_doubling, 0, 10, aperture_text.c_str());

	std::string shutter_text = shutter_speed >= 0.3f ? fmt::format("{:.1f} s", shutter_speed) : fmt::format("1/{:.0f} s", 1.0f / shutter_speed);
	stopSlider("Shutter Speed", &shutter_speed, 1.0f, shutter_steps_per_doubling, 1, 14, shutter_text.c_str());

	std::string iso_text = fmt::format("ISO {:.0f}", iso);
	stopSlider("Sensitivity", &iso, 100.0f, iso_steps_per_doubling, 0, 8, iso_text.c_str());

	ImGui::EndDisabled();

	if (is_camera_mode)
		film_ubo.exposure = 1.0f / EV100ToLuminance(getLuminanceMax(), cameraToEV100(aperture, shutter_speed, iso));

	UI::text("Exposure Multiplier", "%.6f", film_ubo.exposure);

	ImGui::SeparatorText("Film");

	const char *tonemappers[] = {"Disabled", "Uncharted2", "ACES"};
	UI::combo("Tonemapper", &film_ubo.tonemapper_mode, tonemappers, IM_ARRAYSIZE(tonemappers));

	bool use_vignette = film_ubo.use_vignette > 0.5f;
	if (UI::checkbox("Vignette", &use_vignette))
		film_ubo.use_vignette = use_vignette ? 1.0f : 0.0f;

	if (use_vignette)
	{
		UI::sliderFloat("Vignette Radius", &film_ubo.vignette_radius, 0.1f, 1.0f, "%.2f");
		UI::sliderFloat("Vignette Smoothness", &film_ubo.vignette_smoothness, 0.1f, 1.0f, "%.2f");
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
