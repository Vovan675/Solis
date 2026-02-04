#include "pch.h"
#include "Core/Variables.h"

// Startup variables
AutoConVarBool engine_rhi_validation("engine.rhi_validation.enabled", "RHI Validation Enabled", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool engine_rhi_validation_break("engine.rhi_validation_break.enabled", "RHI Validation Break Enabled", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool engine_ray_tracing("engine.ray_tracing.enabled", "Ray Tracing Enabled", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);

// Runtime variables
AutoConVarBool render_vsync("render.vsync.enabled", "Vsync Enabled", false);
AutoConVarBool render_path_tracing("render.path_tracing.enabled", "Path Tracing Enabled", false);
AutoConVarBool render_path_tracing_first_frame("render.path_tracing_first_frame", "Is Path Tracing First Frame", true);
AutoConVarBool render_ddgi("render.ddgi.enabled", "DDGI Enabled", true);
AutoConVarBool render_ddgi_visualize("render.ddgi_visualize.enabled", "DDGI Visualize", true);
AutoConVarBool render_debug_rendering("render.debug_rendering.enabled", "Debug Rendering Enabled", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarInt render_debug_rendering_mode("render.debug_rendering.mode", "Debug Rendering Present Mode", 2, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool render_first_frame("render.first_frame", "Is First Frame", true);
AutoConVarBool render_culling_hiz_debug("render.culling.hiz_debug", "Culling HiZ Debug", false);
AutoConVarBool render_freeze_culling("render.freeze_culling", "Freeze Culling", false);
AutoConVarBool render_shadows("render.shadows.enabled", "Shadows Enabled", false);
AutoConVarBool render_ray_traced_shadows("render.ray_traced_shadows.enabled", "Ray Traced Shadows Enabled", true);
AutoConVarBool render_ssao("render.ssao.enabled", "Screen Space Ambient Occlusion Enabled", true);
AutoConVarBool render_ssr("render.ssr.enabled", "Screen Space Reflections Enabled", false);
AutoConVarBool render_fxaa("render.fxaa.enabled", "FXAA Enabled", true);
AutoConVarBool render_automatic_sun_position("render.automatic_sun_position.enabled", "Automatic Sun Position", true, ConVarFlag::CON_VAR_FLAG_HIDDEN);