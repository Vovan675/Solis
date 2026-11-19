#pragma once
#include "FrameGraph/FrameGraph.h"
#include "Renderers/DebugRenderer.h"
#include "Renderers/OpaqueGeometryPass.h"
#include "Editor/EditorContext.h"
#include "RHI/RayTracing/RayTracingScene.h"
#include "Rendering/Renderer.h"
#include "Rendering/GpuTable.h"

class ShadowRenderer
{
public:
	void addCullViews(GpuTable<CullViewGPU> &cull_views, Camera *camera);
	void addShadowMapPasses(FrameGraph &fg, uint32_t max_draw_calls_count);
	void addRayTracedShadowPasses(FrameGraph &fg, Ref<RayTracingScene> rt_scene, uint32_t sun_light_index);

	// TODO: remove from it, do itsomehow else
	DebugRenderer *debug_renderer;
	Ref<RayTracingScene> ray_tracing_scene;

private:
	void update_cascades(LightComponent &light, glm::vec3 light_dir, Camera *camera);

private:
	struct ShadowViews
	{
		entt::entity light_entity;
		RHITextureRef shadow_map;
		LIGHT_TYPE type;
		uint32_t first_view_id;
	};

	eastl::vector<ShadowViews> shadow_views;
};