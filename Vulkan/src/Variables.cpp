#include "pch.h"
#include "Variables.h"

AutoConVarBool render_vsync("render.vsync.enabled", "Vsync Enabled", true);
AutoConVarBool render_debug_rendering("render.debug_rendering.enabled", "Debug Rendering Enabled", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarInt render_debug_rendering_mode("render.debug_rendering.mode", "Debug Rendering Present Mode", 1, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool render_first_frame("render.first_frame", "Is First Frame", true);
AutoConVarBool render_shadows("render.shadows.enabled", "Shadows Enabled", true);
AutoConVarBool render_ray_traced_shadows("render.ray_traced_shadows.enabled", "Ray Traced Shadows Enabled", false);

AutoConVarBool engine_ray_tracing("engine.ray_tracing.enabled", "Ray Tracing Enabled", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);