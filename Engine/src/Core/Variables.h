#pragma once
#include "ConsoleVariables.h"

enum UpscaleMode
{
	UPSCALE_MODE_OFF = 0,
	UPSCALE_MODE_DLSS = 1,
};

enum DLSSQualityMode
{
	DLSS_MODE_MAX_PERFORMANCE = 0,
	DLSS_MODE_BALANCED,
	DLSS_MODE_MAX_QUALITY,
	DLSS_MODE_ULTRA_PERFORMANCE,
	DLSS_MODE_DLAA,
};

// Startup variables
extern AutoConVarBool engine_rhi_validation;
extern AutoConVarBool engine_rhi_validation_break;
extern AutoConVarBool engine_ray_tracing;
extern AutoConVarBool engine_reimport_assets;
extern AutoConVarBool engine_shader_debug_info;
extern AutoConVarBool engine_streamline_enabled;

// Runtime variables
extern AutoConVarBool render_vsync;
extern AutoConVarBool render_path_tracing;
extern AutoConVarBool render_path_tracing_first_frame;
extern AutoConVarBool render_ddgi;
extern AutoConVarBool render_sky;
extern AutoConVarBool render_lighting_only;
extern AutoConVarBool render_ddgi_visualize;
extern AutoConVarBool render_debug_rendering;
extern AutoConVarInt render_debug_rendering_mode;
extern AutoConVarBool render_first_frame;
extern AutoConVarBool render_culling_hiz_debug;
extern AutoConVarBool render_freeze_culling;
extern AutoConVarBool render_shadows;
extern AutoConVarBool render_ray_traced_shadows;
extern AutoConVarBool render_ssao;
extern AutoConVarBool render_ssr;
extern AutoConVarBool render_fxaa;
extern AutoConVarInt render_upscale_mode;
extern AutoConVarInt render_dlss_mode;
extern AutoConVarBool render_automatic_sun_position;
extern AutoConVarBool render_meshlets_use_mesh_shaders;
extern AutoConVarBool render_meshlets_bvh_visualize;
extern AutoConVarInt  render_meshlets_bvh_visualize_depth;

// Asset import
extern AutoConVarInt  engine_gltf_import_threads;
