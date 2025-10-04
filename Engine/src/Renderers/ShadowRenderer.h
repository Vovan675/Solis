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

	void addShadowMapPasses(FrameGraph &fg, const std::vector<RenderBatch> &batches);
	void addRayTracedShadowPasses(FrameGraph &fg);

	// TODO: remove from it, do itsomehow else
	DebugRenderer *debug_renderer;
	EntityRenderer *entity_renderer;
	Camera *camera;
	Ref<RayTracingScene> ray_tracing_scene;

private:
	void update_cascades(LightComponent &light, glm::vec3 light_dir);

private:
	RHIShaderRef shadows_vertex_shader;
	RHIShaderRef shadows_fragment_shader_point;
	RHIShaderRef shadows_fragment_shader_directional;

	RHIShaderRef raygen_shader;
	RHIShaderRef miss_shader;
	RHIShaderRef closest_hit_shader;

	RHITextureRef storage_image;
};