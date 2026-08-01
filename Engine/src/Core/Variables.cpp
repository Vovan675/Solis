#include "pch.h"
#include "Core/Variables.h"

// Startup variables
AutoConVarBool engine_rhi_validation("engine.rhi_validation.enabled", "RHI Validation Enabled", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool engine_rhi_validation_break("engine.rhi_validation_break.enabled", "RHI Validation Break Enabled", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool engine_ray_tracing("engine.ray_tracing.enabled", "Ray Tracing Enabled", true, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool engine_reimport_assets("engine.reimport_assets", "Force reimport all assets from source, ignoring binary cache", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool engine_shader_debug_info("engine.shader.debug_info", "Embed debug info into shaders (slower shaders compilation)", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool engine_streamline_enabled("engine.streamline.enabled", "Initialize Streamline (DLSS and other features) at startup", true, ConVarFlag::CON_VAR_FLAG_HIDDEN);

// Runtime variables
AutoConVarBool render_vsync("render.vsync.enabled", "Vsync Enabled", false);
AutoConVarBool render_path_tracing("render.path_tracing.enabled", "Path Tracing Enabled", false);
AutoConVarBool render_path_tracing_first_frame("render.path_tracing_first_frame", "Is Path Tracing First Frame", true);
AutoConVarBool render_ddgi("render.ddgi.enabled", "DDGI Enabled", true);
AutoConVarBool render_sky("render.sky.enabled", "Sky Enabled", true);
AutoConVarBool render_lighting_only("render.debug.lighting_only", "Lighting Only", false);
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
AutoConVarInt render_upscale_mode("render.upscale.mode", "Upscaling (0=Off, 1=DLSS)", 0, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarInt render_dlss_mode("render.upscale.dlss_mode", "DLSS Mode", 2, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool render_automatic_sun_position("render.automatic_sun_position.enabled", "Automatic Sun Position", true, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool render_meshlets_use_mesh_shaders("render.meshlets.use_mesh_shaders", "Use Mesh Shaders for Meshlet Rendering", true);
AutoConVarBool render_meshlets_bvh_visualize("render.meshlets.bvh_visualize", "Draw BVH node spheres for all scene meshes", false);
AutoConVarInt  render_meshlets_bvh_visualize_depth("render.meshlets.bvh_visualize_depth", "BVH depth to visualize (-1 = all, 0 = root)", -1);

// high thread counts leads to more maximum used RAM on importing
AutoConVarInt engine_gltf_import_threads("engine.gltf.import_threads", "Max parallel threads for glTF mesh building (0 = hw_concurrency/2)", 10);
