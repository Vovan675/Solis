#include "pch.h"
#include "Core/Variables.h"

RenderSettings gRenderSettings;

// Startup variables
AutoConVarBool engine_rhi_validation("engine.rhi.validation", "RHI Validation Enabled", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool engine_rhi_validation_break("engine.rhi.validation_break", "RHI Validation Break Enabled", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool engine_ray_tracing("engine.ray_tracing", "Ray Tracing Enabled", true, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool engine_assets_reimport("engine.assets.reimport", "Force reimport all assets from source, ignoring binary cache", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool engine_shader_debug_info("engine.shader.debug_info", "Embed debug info into shaders (slower shaders compilation)", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool engine_streamline("engine.streamline", "Initialize Streamline (DLSS and other features) at startup", true, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarString engine_startup_scene("engine.startup_scene", "Scene opened at startup", "", ConVarFlag::CON_VAR_FLAG_HIDDEN);

// Runtime variables
AutoConVarBool render_vsync("render.vsync", "VSync", false);
AutoConVarBool render_path_tracing_first_frame("render.path_tracing.first_frame", "Is Path Tracing First Frame", true, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool render_lighting_only("render.debug.lighting_only", "Lighting Only", false);
AutoConVarBool render_ddgi_visualize("render.ddgi.visualize", "DDGI Visualize", false);
AutoConVarInt render_ddgi_visualize_mode("render.ddgi.visualize_mode", "DDGI Visualize Mode", 0, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool render_debug_rendering("render.debug.rendering", "Debug Rendering", false, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarInt render_debug_rendering_mode("render.debug.rendering_mode", "Debug Rendering Present Mode", 2, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool render_first_frame("render.first_frame", "Is First Frame", true, ConVarFlag::CON_VAR_FLAG_HIDDEN);
AutoConVarBool render_culling_hiz_debug("render.culling.hiz_debug", "Culling HiZ Debug", false);
AutoConVarBool render_culling_freeze("render.culling.freeze", "Freeze Culling", false);
AutoConVarBool render_meshlets_mesh_shaders("render.meshlets.mesh_shaders", "Mesh Shaders", true);
AutoConVarBool render_meshlets_bvh_visualize("render.meshlets.bvh_visualize", "Draw BVH Spheres", false);
AutoConVarInt render_meshlets_bvh_visualize_depth("render.meshlets.bvh_visualize_depth", "BVH Depth", -1);

// high thread counts leads to more maximum used RAM on importing
AutoConVarInt engine_gltf_import_threads("engine.gltf.import_threads", "glTF Import Threads", 10);
