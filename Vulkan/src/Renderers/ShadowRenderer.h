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
	std::shared_ptr<RayTracingScene> ray_tracing_scene;

private:
	void update_cascades(LightComponent &light, glm::vec3 light_dir);

private:
	std::shared_ptr<RHIShader> shadows_vertex_shader;
	std::shared_ptr<RHIShader> shadows_fragment_shader_point;
	std::shared_ptr<RHIShader> shadows_fragment_shader_directional;

	std::shared_ptr<RHIShader> raygen_shader;
	std::shared_ptr<RHIShader> miss_shader;
	std::shared_ptr<RHIShader> closest_hit_shader;

	std::shared_ptr<RHITexture> storage_image;
};