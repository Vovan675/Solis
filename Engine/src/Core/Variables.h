#pragma once
#include "ConsoleVariables.h"
#include "Reflection.h"
#include "Utils/Math.h"

class RHITexture;

enum UpscaleMode
{
	UPSCALE_MODE_OFF = 0,
	UPSCALE_MODE_DLSS = 1,
};
inline const char *const upscale_mode_items[] = {"Off", "DLSS"};

enum DLSSQualityMode
{
	DLSS_MODE_MAX_PERFORMANCE = 0,
	DLSS_MODE_BALANCED,
	DLSS_MODE_MAX_QUALITY,
	DLSS_MODE_ULTRA_PERFORMANCE,
	DLSS_MODE_DLAA,
};
inline const char *const dlss_quality_items[] = {"Performance", "Balanced", "Quality", "Ultra Performance", "DLAA"};

enum SkyMode
{
	SKY_MODE_CUBEMAP = 0,
	SKY_MODE_PROCEDURAL,
};
inline const char *const sky_mode_items[] = {"HDRI", "Procedural"};

enum ExposureMode
{
	EXPOSURE_MODE_EV100 = 0,
	EXPOSURE_MODE_CAMERA,
};
inline const char *const exposure_mode_items[] = {"EV100", "Camera"};

enum TonemapperMode
{
	TONEMAPPER_DISABLED = 0,
	TONEMAPPER_PBR_NEUTRAL,
	TONEMAPPER_ACES,
	TONEMAPPER_UCHIMURA,
	TONEMAPPER_AGX,
};
inline const char *const tonemapper_items[] = {"Disabled", "Khronos PBR Neutral", "ACES", "Uchimura (GT Sport)", "AgX"};

enum ScreenshotFormat
{
	SCREENSHOT_FORMAT_PNG_EXR = 0,
	SCREENSHOT_FORMAT_PNG,
	SCREENSHOT_FORMAT_EXR,
};
inline const char *const screenshot_format_items[] = {"PNG + EXR", "PNG", "EXR"};

struct SkySettings
{
	bool enabled = true;
	SkyMode mode = SKY_MODE_CUBEMAP;
	AssetReference hdri = AssetReference("assets/textures/hdri/kloppenheim_06_puresky_4k.hdr");
	float intensity = 15000.0f;
	float procedural_luminance = 1000.0f;
	bool automatic_sun_position = true;
	glm::vec3 sun_direction = glm::vec3(1.0f, 0.7f, 0.0f);

	float getIntensity() const
	{
		return mode == SKY_MODE_CUBEMAP ? intensity : 1.0f;
	}
};

REFLECT_BEGIN(SkySettings)
	REFLECT_CATEGORY("Sky"),
	REFLECT_FIELD(enabled).label("Enable Sky"),
	REFLECT_FIELD(mode).label("Source").items(sky_mode_items).radio()
		.EDIT_IF(owner.enabled),
	REFLECT_FIELD(hdri).label("HDRI").asset<RHITexture>()
		.EDIT_IF(owner.enabled && owner.mode == SKY_MODE_CUBEMAP),
	REFLECT_FIELD(intensity).label("HDRI Intensity").range(1.0f, 100000.0f).format("%.0f nits").logarithmic()
		.EDIT_IF(owner.enabled && owner.mode == SKY_MODE_CUBEMAP),
	REFLECT_FIELD(procedural_luminance).range(1.0f, 100000.0f).format("%.0f nits").logarithmic()
		.EDIT_IF(owner.enabled && owner.mode == SKY_MODE_PROCEDURAL),
	REFLECT_FIELD(automatic_sun_position)
		.EDIT_IF(owner.enabled && owner.mode == SKY_MODE_PROCEDURAL),
	REFLECT_FIELD(sun_direction).range(-1.0f, 1.0f).format("%.2f")
		.EDIT_IF(owner.enabled && owner.mode == SKY_MODE_PROCEDURAL && !owner.automatic_sun_position),
REFLECT_END()

struct UchimuraSettings
{
	float max_brightness = 1.0f;
	float contrast = 1.0f;
	float linear_section_start = 0.22f;
	float linear_section_length = 0.4f;
	float black_tightness = 1.33f;
	float pedestal = 0.0f;
};

REFLECT_BEGIN(UchimuraSettings)
	REFLECT_FIELD(max_brightness).range(0.5f, 2.0f),
	REFLECT_FIELD(contrast).range(0.5f, 2.0f),
	REFLECT_FIELD(linear_section_start).range(0.01f, 0.5f),
	REFLECT_FIELD(linear_section_length).range(0.0f, 0.9f),
	REFLECT_FIELD(black_tightness).range(1.0f, 2.0f),
	REFLECT_FIELD(pedestal).range(0.0f, 0.1f),
REFLECT_END()

struct FilmSettings
{
	ExposureMode exposure_mode = EXPOSURE_MODE_EV100;
	float ev100 = 14.0f;
	float aperture = 2.8f;
	float shutter_speed = 1.0f / 8192.0f;
	float iso = 100.0f;
	TonemapperMode tonemapper = TONEMAPPER_DISABLED;
	UchimuraSettings uchimura;
	bool vignette = false;
	float vignette_radius = 0.7f;
	float vignette_smoothness = 0.2f;

	float getEV100() const
	{
		if (exposure_mode == EXPOSURE_MODE_CAMERA)
			return Math::cameraToEV100(aperture, shutter_speed, iso);
		return ev100;
	}

	float getExposure() const
	{
		return 1.0f / Math::ev100ToLuminance(Math::getLuminanceMax(), getEV100());
	}
};

REFLECT_BEGIN(FilmSettings)
	REFLECT_CATEGORY("Exposure"),
	REFLECT_FIELD(exposure_mode).label("Set By").items(exposure_mode_items).radio(),
	REFLECT_FIELD(ev100).label("EV100").range(-6.0f, 20.0f).format("%.2f EV")
		.EDIT_IF(owner.exposure_mode == EXPOSURE_MODE_EV100),
	REFLECT_FIELD(aperture).range(1.0f, 32.0f).format("f/%.1f").logarithmic()
		.EDIT_IF(owner.exposure_mode == EXPOSURE_MODE_CAMERA),
	REFLECT_FIELD(shutter_speed).range(1.0f / 8192.0f, 30.0f).format("%.4f s").logarithmic()
		.EDIT_IF(owner.exposure_mode == EXPOSURE_MODE_CAMERA),
	REFLECT_FIELD(iso).label("ISO").range(100.0f, 25600.0f).format("ISO %.0f").logarithmic()
		.EDIT_IF(owner.exposure_mode == EXPOSURE_MODE_CAMERA),
	REFLECT_CATEGORY("Film"),
	REFLECT_FIELD(tonemapper).items(tonemapper_items),
	REFLECT_FIELD(uchimura).label("Uchimura").EDIT_IF(owner.tonemapper == TONEMAPPER_UCHIMURA),
	REFLECT_FIELD(vignette),
	REFLECT_FIELD(vignette_radius).range(0.1f, 1.0f).format("%.2f")
		.EDIT_IF(owner.vignette),
	REFLECT_FIELD(vignette_smoothness).range(0.1f, 1.0f).format("%.2f")
		.EDIT_IF(owner.vignette),
REFLECT_END()

struct ColorGradingSettings
{
	float temperature = 6500.0f;
	float tint = 0.0f;
	float contrast = 1.0f;
	float saturation = 1.0f;
	float shadows = 0.0f;
	float midtones = 0.0f;
	float highlights = 0.0f;
};

REFLECT_BEGIN(ColorGradingSettings)
	REFLECT_CATEGORY("Color Grading"),
	REFLECT_FIELD(temperature).range(4000.0f, 10000.0f).format("%.0f K"),
	REFLECT_FIELD(tint).range(-0.5f, 0.5f),
	REFLECT_FIELD(contrast).range(0.5f, 1.5f),
	REFLECT_FIELD(saturation).range(0.0f, 2.0f),
	REFLECT_FIELD(shadows).range(-2.0f, 2.0f).format("%.2f EV"),
	REFLECT_FIELD(midtones).range(-2.0f, 2.0f).format("%.2f EV"),
	REFLECT_FIELD(highlights).range(-2.0f, 2.0f).format("%.2f EV"),
REFLECT_END()

struct BloomSettings
{
	bool enabled = false;
	float intensity = 0.1f;
	float threshold = 3.0f;
	float scatter = 0.7f;
	uint32_t mip_count = 9;
};

REFLECT_BEGIN(BloomSettings)
	REFLECT_CATEGORY("Bloom"),
	REFLECT_FIELD(enabled).label("Enable Bloom"),
	REFLECT_FIELD(intensity).range(0.0f, 1.0f).format("%.3f").EDIT_IF(owner.enabled),
	REFLECT_FIELD(threshold).range(0.0f, 10.0f).format("%.2f").EDIT_IF(owner.enabled),
	REFLECT_FIELD(scatter).range(0.0f, 1.0f).format("%.2f").EDIT_IF(owner.enabled),
	REFLECT_FIELD(mip_count).range(1.0f, 12.0f).tooltip("More mips = wider effect").EDIT_IF(owner.enabled),
REFLECT_END()

enum RTXDIBiasCorrection
{
	BIAS_CORRECTION_OFF = 0,
	BIAS_CORRECTION_BASIC,
	BIAS_CORRECTION_PAIRWISE,
	BIAS_CORRECTION_RAYTRACED,
};
inline const char *const rtxdi_bias_correction_items[] = {"Off", "Basic", "Pairwise", "Ray Traced"};

struct RTXDISettings
{
	int max_history_length = 20;
	int spatial_samples = 1;
	int disocclusion_boost_samples = 8;
	float sampling_radius = 32.0f;
	float depth_threshold = 0.1f;
	float normal_threshold = 0.5f;
	RTXDIBiasCorrection bias_correction = BIAS_CORRECTION_PAIRWISE;
	bool material_similarity_test = true;
	bool visibility_shortcut = false;
	bool permutation_sampling = false;
	bool discount_naive_samples = true;
	bool reuse_final_visibility = false;
};

REFLECT_BEGIN(RTXDISettings)
	REFLECT_FIELD(max_history_length).range(1.0f, 64.0f),
	REFLECT_FIELD(spatial_samples).range(0.0f, 32.0f),
	REFLECT_FIELD(disocclusion_boost_samples).range(1.0f, 32.0f),
	REFLECT_FIELD(sampling_radius).range(1.0f, 64.0f).format("%.0f px"),
	REFLECT_FIELD(depth_threshold).range(0.0f, 1.0f),
	REFLECT_FIELD(normal_threshold).range(0.0f, 1.0f),
	REFLECT_FIELD(bias_correction).items(rtxdi_bias_correction_items),
	REFLECT_FIELD(material_similarity_test),
	REFLECT_FIELD(visibility_shortcut),
	REFLECT_FIELD(permutation_sampling),
	REFLECT_FIELD(discount_naive_samples),
	REFLECT_FIELD(reuse_final_visibility),
REFLECT_END()

struct RelaxSettings
{
	uint32_t max_accumulated_frames = 20;
	uint32_t max_fast_accumulated_frames = 4;
	float diffuse_prepass_blur_radius = 30.0f;
	float specular_prepass_blur_radius = 50.0f;
	float min_hit_distance_weight = 0.1f;
	float diffuse_phi_luminance = 1.0f;
	float specular_phi_luminance = 1.0f;
	uint32_t atrous_iterations = 5;
	float depth_threshold = 0.003f;
	float lobe_angle_fraction = 0.5f;
	float roughness_fraction = 0.15f;
	bool anti_firefly = true;
};

REFLECT_BEGIN(RelaxSettings)
	REFLECT_FIELD(max_accumulated_frames).range(1.0f, 64.0f),
	REFLECT_FIELD(max_fast_accumulated_frames).range(0.0f, 64.0f),
	REFLECT_FIELD(diffuse_prepass_blur_radius).range(0.0f, 50.0f).format("%.0f px"),
	REFLECT_FIELD(specular_prepass_blur_radius).range(0.0f, 50.0f).format("%.0f px"),
	REFLECT_FIELD(min_hit_distance_weight).range(0.0f, 0.2f),
	REFLECT_FIELD(diffuse_phi_luminance).range(0.0f, 10.0f),
	REFLECT_FIELD(specular_phi_luminance).range(0.0f, 10.0f),
	REFLECT_FIELD(atrous_iterations).range(2.0f, 8.0f),
	REFLECT_FIELD(depth_threshold).range(0.0f, 0.1f).format("%.3f"),
	REFLECT_FIELD(lobe_angle_fraction).range(0.0f, 1.0f).format("%.2f"),
	REFLECT_FIELD(roughness_fraction).range(0.0f, 1.0f).format("%.2f"),
	REFLECT_FIELD(anti_firefly),
REFLECT_END()

enum ConfidenceVisualize
{
	CONFIDENCE_VISUALIZE_OFF = 0,
	CONFIDENCE_VISUALIZE_DIFFUSE,
	CONFIDENCE_VISUALIZE_SPECULAR,
};
inline const char *const confidence_visualize_items[] = {"Off", "Diffuse", "Specular"};

struct ConfidenceSettings
{
	bool enabled = true;
	float sensitivity = 8.0f;
	float darkness_bias_ev = -12.0f;
	float history_length = 0.75f;
	ConfidenceVisualize visualize = CONFIDENCE_VISUALIZE_OFF;
};

REFLECT_BEGIN(ConfidenceSettings)
	REFLECT_FIELD(enabled).label("Enable Confidence"),
	REFLECT_FIELD(sensitivity).range(1.0f, 20.0f).format("%.1f").EDIT_IF(owner.enabled),
	REFLECT_FIELD(darkness_bias_ev).label("Darkness Bias").range(-16.0f, -4.0f).format("%.0f EV").EDIT_IF(owner.enabled),
	REFLECT_FIELD(history_length).range(0.0f, 3.0f).format("%.2f frames").EDIT_IF(owner.enabled),
	REFLECT_FIELD(visualize).items(confidence_visualize_items).notSerialized().EDIT_IF(owner.enabled),
REFLECT_END()


enum DirectLightingMode
{
	DIRECT_LIGHTING_DEFERRED = 0,
	DIRECT_LIGHTING_RTXDI,
};
inline const char *const direct_lighting_mode_items[] = {"Deferred", "RTXDI"};

enum DenoiserMode
{
	DENOISER_OFF = 0,
	DENOISER_RELAX,
	DENOISER_REFERENCE,
};
inline const char *const denoiser_mode_items[] = {"Off", "RELAX", "Reference"};

struct LightingSettings
{
	DirectLightingMode direct = DIRECT_LIGHTING_DEFERRED;
	DenoiserMode denoiser = DENOISER_OFF;
	bool denoiser_validation = false;
	float denoiser_split_screen = 0.0f;
	RTXDISettings rtxdi;
	RelaxSettings relax;
	ConfidenceSettings confidence;
};

REFLECT_BEGIN(LightingSettings)
	REFLECT_CATEGORY("Lighting"),
	REFLECT_FIELD(direct).label("Direct Lighting").items(direct_lighting_mode_items).radio(),
	REFLECT_FIELD(denoiser).items(denoiser_mode_items).EDIT_IF(owner.direct == DIRECT_LIGHTING_RTXDI),
	REFLECT_FIELD(denoiser_validation).notSerialized().EDIT_IF(owner.direct == DIRECT_LIGHTING_RTXDI && owner.denoiser == DENOISER_RELAX),
	REFLECT_FIELD(denoiser_split_screen).range(0.0f, 1.0f).format("%.2f").notSerialized().EDIT_IF(owner.direct == DIRECT_LIGHTING_RTXDI && owner.denoiser == DENOISER_RELAX),
	REFLECT_FIELD(rtxdi).label("RTXDI").EDIT_IF(owner.direct == DIRECT_LIGHTING_RTXDI),
	REFLECT_FIELD(relax).label("RELAX").EDIT_IF(owner.direct == DIRECT_LIGHTING_RTXDI && owner.denoiser == DENOISER_RELAX),
	REFLECT_FIELD(confidence).label("History Confidence").EDIT_IF(owner.direct == DIRECT_LIGHTING_RTXDI && owner.denoiser == DENOISER_RELAX),
REFLECT_END()

struct SSAOSettings
{
	bool enabled = true;
	int samples = 64;
	float radius = 0.5f;
};

REFLECT_BEGIN(SSAOSettings)
	REFLECT_CATEGORY("SSAO"),
	REFLECT_FIELD(enabled).label("Enable SSAO"),
	REFLECT_FIELD(samples).range(2.0f, 64.0f).EDIT_IF(owner.enabled),
	REFLECT_FIELD(radius).range(0.01f, 1.0f).format("%.2f m").EDIT_IF(owner.enabled),
REFLECT_END()

struct ShadowSettings
{
	bool enabled = true;
	bool ray_traced = true;
	float cascades_distance = 200.0f;
};

REFLECT_BEGIN(ShadowSettings)
	REFLECT_CATEGORY("Shadows"),
	REFLECT_FIELD(enabled).label("Enable Shadows"),
	REFLECT_FIELD(ray_traced).label("Ray Traced Shadows").EDIT_IF(owner.enabled),
	REFLECT_FIELD(cascades_distance).range(10.0f, 3000.0f).format("%.0f m").EDIT_IF(owner.enabled && !owner.ray_traced),
REFLECT_END()

struct SSRSettings
{
	bool enabled = false;
};

REFLECT_BEGIN(SSRSettings)
	REFLECT_CATEGORY("SSR"),
	REFLECT_FIELD(enabled).label("Enable SSR"),
REFLECT_END()

struct AntiAliasingSettings
{
	UpscaleMode upscale = UPSCALE_MODE_OFF;
	DLSSQualityMode dlss_quality = DLSS_MODE_MAX_QUALITY;
	bool fxaa = true;
};

REFLECT_BEGIN(AntiAliasingSettings)
	REFLECT_CATEGORY("Anti-Aliasing"),
	REFLECT_FIELD(upscale).items(upscale_mode_items),
	REFLECT_FIELD(dlss_quality).label("DLSS Quality").items(dlss_quality_items).EDIT_IF(owner.upscale == UPSCALE_MODE_DLSS),
	REFLECT_FIELD(fxaa).label("FXAA").EDIT_IF(owner.upscale == UPSCALE_MODE_OFF),
REFLECT_END()

struct DDGISettings
{
	bool enabled = false;
	glm::vec3 origin = glm::vec3(0.0f, 5.0f, 0.0f);
	glm::ivec3 size = glm::ivec3(16, 8, 16);
	glm::vec3 spacing = glm::vec3(0.5f, 1.0f, 0.5f);
	int probes_per_frame = 1024;
	bool use_relocation = true;
	bool use_classification = true;
};

REFLECT_BEGIN(DDGISettings)
	REFLECT_CATEGORY("DDGI"),
	REFLECT_FIELD(enabled).label("Enable DDGI"),
	REFLECT_CATEGORY("DDGI - Volume"),
	REFLECT_FIELD(origin).range(-10.0f, 10.0f).format("%.2f").EDIT_IF(owner.enabled),
	REFLECT_FIELD(size).range(1.0f, 32.0f).EDIT_IF(owner.enabled),
	REFLECT_FIELD(spacing).range(0.05f, 5.0f).format("%.2f m").EDIT_IF(owner.enabled),
	REFLECT_CATEGORY("DDGI - Update"),
	REFLECT_FIELD(probes_per_frame).range(16.0f, 16384.0f).logarithmic().tooltip("Must be a power of two").EDIT_IF(owner.enabled),
	REFLECT_FIELD(use_relocation).EDIT_IF(owner.enabled),
	REFLECT_FIELD(use_classification).EDIT_IF(owner.enabled),
REFLECT_END()

// Authored render settings that are saved
struct RenderSettings
{
	bool path_tracing = false;

	SkySettings sky;
	FilmSettings film;
	ColorGradingSettings color_grading;
	BloomSettings bloom;
	LightingSettings lighting;
	SSAOSettings ssao;
	ShadowSettings shadows;
	SSRSettings ssr;
	AntiAliasingSettings anti_aliasing;
	DDGISettings ddgi;
};

REFLECT_BEGIN(RenderSettings)
	REFLECT_FIELD(path_tracing),
	REFLECT_FIELD(sky),
	REFLECT_FIELD(film),
	REFLECT_FIELD(color_grading),
	REFLECT_FIELD(bloom),
	REFLECT_FIELD(lighting),
	REFLECT_FIELD(ssao),
	REFLECT_FIELD(shadows),
	REFLECT_FIELD(ssr),
	REFLECT_FIELD(anti_aliasing),
	REFLECT_FIELD(ddgi),
REFLECT_END()

extern RenderSettings gRenderSettings;

// Access is global and by system name. In future the storage can easily move (per view)
#define GFXOPTIONS(T) gRenderSettings.T

// ConVars mostly used for debug features and non preset settings or runtime switches

// Startup variables
extern AutoConVarBool engine_rhi_validation;
extern AutoConVarBool engine_rhi_validation_break;
extern AutoConVarBool engine_ray_tracing;
extern AutoConVarBool engine_assets_reimport;
extern AutoConVarBool engine_shader_debug_info;
extern AutoConVarBool engine_streamline;
extern AutoConVarString engine_startup_scene;
extern AutoConVarBool engine_quit;

// Runtime variables
extern AutoConVarBool render_vsync;
extern AutoConVarInt render_virtual_width;
extern AutoConVarInt render_virtual_height;
extern AutoConVarBool render_path_tracing_first_frame;
extern AutoConVarBool render_lighting_only;
extern AutoConVarBool render_keep_persistent_resources;
extern AutoConVarBool render_ddgi_visualize;
extern AutoConVarInt render_ddgi_visualize_mode;
extern AutoConVarBool render_debug_rendering;
extern AutoConVarInt render_debug_rendering_mode;
extern AutoConVarBool render_first_frame;
extern AutoConVarBool render_culling_hiz_debug;
extern AutoConVarBool render_culling_freeze;
extern AutoConVarBool render_meshlets_mesh_shaders;
extern AutoConVarBool render_meshlets_bvh_visualize;
extern AutoConVarInt render_meshlets_bvh_visualize_depth;

// Screenshots
extern AutoConVarString render_screenshot_name;
extern AutoConVarInt render_screenshot_frames;
extern AutoConVarInt render_screenshot_format;
extern AutoConVarBool render_screenshot_raw;
extern AutoConVarBool render_screenshot;
extern AutoConVarBool engine_quit_after_screenshot;

// Asset import
extern AutoConVarInt engine_gltf_import_threads;
