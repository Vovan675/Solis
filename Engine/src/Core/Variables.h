#pragma once
#include "ConsoleVariables.h"

// Startup variables
extern AutoConVarBool engine_rhi_validation;
extern AutoConVarBool engine_rhi_validation_break;
extern AutoConVarBool engine_ray_tracing;
extern AutoConVarBool engine_reimport_assets;

// Runtime variables
extern AutoConVarBool render_vsync;
extern AutoConVarBool render_path_tracing;
extern AutoConVarBool render_path_tracing_first_frame;
extern AutoConVarBool render_ddgi;
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
extern AutoConVarBool render_automatic_sun_position;
extern AutoConVarBool render_meshlets_use_mesh_shaders;
extern AutoConVarBool render_meshlets_bvh_visualize;
extern AutoConVarInt  render_meshlets_bvh_visualize_depth;

// Asset import
extern AutoConVarInt  engine_gltf_import_threads;
