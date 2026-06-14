#pragma once
#include "FrameGraph/FrameGraph.h"
#include "Renderers/DebugRenderer.h"
#include "Renderers/OpaqueGeometryPass.h"
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

private:
	RHIShaderRef raygen_shader;
	RHIShaderRef miss_shader;
	RHIShaderRef closest_hit_shader;

	RHITextureRef cascade_hiz;
	RHITextureRef storage_image;
};