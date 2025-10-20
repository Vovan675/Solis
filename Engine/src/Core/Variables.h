#pragma once
#include "ConsoleVariables.h"

// Startup variables
extern AutoConVarBool engine_rhi_validation;
extern AutoConVarBool engine_ray_tracing;

// Runtime variables
extern AutoConVarBool render_vsync;
extern AutoConVarBool render_debug_rendering;
extern AutoConVarInt render_debug_rendering_mode;
extern AutoConVarBool render_first_frame;
extern AutoConVarBool render_shadows;
extern AutoConVarBool render_ray_traced_shadows;
extern AutoConVarBool render_ssao;
extern AutoConVarBool render_ssr;
extern AutoConVarBool render_fxaa;
extern AutoConVarBool render_automatic_sun_position;
