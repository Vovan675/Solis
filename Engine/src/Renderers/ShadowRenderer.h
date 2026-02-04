#pragma once
#include "FrameGraph/FrameGraph.h"
#include "Renderers/DebugRenderer.h"
#include "Renderers/EntityRenderer.h"
#include "Editor/EditorContext.h"
#include "RHI/RayTracing/RayTracingScene.h"
#include "Rendering/Renderer.h"

class ShadowRenderer
{
public:
	ShadowRenderer();

	void addShadowMapPasses(FrameGraph &fg, uint32_t max_draw_calls_count);
	void addRayTracedShadowPasses(FrameGraph &fg, Ref<RayTracingScene> rt_scene);

	// TODO: remove from it, do itsomehow else
	DebugRenderer *debug_renderer;
	Ref<RayTracingScene> ray_tracing_scene;

	void updateShadows(Camera *camera);
private:
	void update_cascades(LightComponent &light, glm::vec3 light_dir, Camera *camera);

	void create_draw_calls(FrameGraph &fg, uint32_t max_draw_calls_count, uint32_t pass_mask, glm::float4x4 view_projection);

private:
	RHIShaderRef shadows_vertex_shader;
	RHIShaderRef shadows_fragment_shader_point;
	RHIShaderRef shadows_fragment_shader_directional;

	RHIShaderRef raygen_shader;
	RHIShaderRef miss_shader;
	RHIShaderRef closest_hit_shader;

	RHITextureRef storage_image;
};