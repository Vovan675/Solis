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
	TONEMAPPER_UNCHARTED2,
	TONEMAPPER_ACES,
};
inline const char *const tonemapper_items[] = {"Disabled", "Uncharted 2", "ACES"};

struct SkySettings
{
	bool enabled = true;
	SkyMode mode = SKY_MODE_CUBEMAP;
	AssetReference hdri = AssetReference("assets/kloppenheim_06_puresky_4k.hdr");
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

struct FilmSettings
{
	ExposureMode exposure_mode = EXPOSURE_MODE_EV100;
	float ev100 = 14.0f;
	float aperture = 2.8f;
	float shutter_speed = 1.0f / 8192.0f;
	float iso = 100.0f;
	TonemapperMode tonemapper = TONEMAPPER_DISABLED;
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
	REFLECT_FIELD(vignette),
	REFLECT_FIELD(vignette_radius).range(0.1f, 1.0f).format("%.2f")
		.EDIT_IF(owner.vignette),
	REFLECT_FIELD(vignette_smoothness).range(0.1f, 1.0f).format("%.2f")
		.EDIT_IF(owner.vignette),
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
};

REFLECT_BEGIN(ShadowSettings)
	REFLECT_CATEGORY("Shadows"),
	REFLECT_FIELD(enabled).label("Enable Shadows"),
	REFLECT_FIELD(ray_traced).label("Ray Traced Shadows").EDIT_IF(owner.enabled),
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
	bool use_fixed_rays = false;
	bool trace_random_direction = true;
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
	REFLECT_FIELD(use_fixed_rays).EDIT_IF(owner.enabled),
	REFLECT_FIELD(trace_random_direction).EDIT_IF(owner.enabled),
REFLECT_END()

// Authored render settings that are saved
struct RenderSettings
{
	bool path_tracing = false;

	SkySettings sky;
	FilmSettings film;
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

// Runtime variables
extern AutoConVarBool render_vsync;
extern AutoConVarBool render_path_tracing_first_frame;
extern AutoConVarBool render_lighting_only;
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

// Asset import
extern AutoConVarInt engine_gltf_import_threads;
