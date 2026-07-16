#pragma once
#include "RHI/DynamicRHI.h"
#include "Rendering/Mesh.h"
#include "Rendering/GeometryStreaming.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "renderers/LutRenderer.h"
#include "renderers/IrradianceRenderer.h"
#include "renderers/PrefilterRenderer.h"
#include "renderers/SkyRenderer.h"
#include "renderers/MeshRenderer.h"
#include "imgui/ImGuiWrapper.h"
#include "renderers/GBufferPass.h"
#include "renderers/DefferedLightingRenderer.h"
#include "renderers/DefferedCompositeRenderer.h"
#include "renderers/PostProcessingRenderer.h"
#include "renderers/DebugRenderer.h"
#include "renderers/ShadowRenderer.h"
#include "renderers/SSAORenderer.h"
#include "renderers/SSRRenderer.h"
#include "renderers/DDGIRenderer.h"
#include "renderers/UpscaleRenderer.h"

#include "renderers/PathTracingRenderer.h"
#include "ShaderStructs.h"
#include "GpuTable.h"

class Asset;

class SceneRenderer : public RefCounted
{
public:
	SceneRenderer();
	~SceneRenderer();

	void setScene(Ref<Scene> scene);
	void render(Camera *camera, RHITextureRef result_texture);

	Ref<RayTracingScene> getCurrentRayTracingScene() const { return rt_scene; }
public:
	friend class EditorApplication;

	void render_deferred(Camera *camera, FrameGraph &frame_graph);
	void render_path_traced(Camera *camera, FrameGraph &frame_graph);
	void update(Camera *camera);

	void on_mesh_renderer_constructed(entt::registry &registry, entt::entity entity);
	void on_mesh_renderer_destroyed(entt::registry &registry, entt::entity entity);
	void free_instances(entt::entity entity);

	void on_asset_pre_reimport(Asset *asset);
	void on_asset_post_reimport(Asset *asset);

	void gpu_frame_cull(FrameGraph &frame_graph);

	Camera *main_view_camera = nullptr;

	Ref<Scene> scene;
	Ref<RayTracingScene> rt_scene;
	GeometryStreaming geometry_streaming;

	eastl::vector<FrustumDataGPU> frustums;

	struct InstanceRange { uint32_t start; uint32_t count; };
	eastl::hash_map<entt::entity, InstanceRange> entity_instances;
	eastl::hash_set<entt::entity> moved_last_frame_entities;

	GpuTable<FrustumDataGPU> frustums_table;
	GpuTable<MaterialGPU> materials_table;
	GpuTable<MeshGPU> meshes_table;
	GpuTable<InstanceGPU> instances_table;

	uint32_t indirect_draw_calls_max_count;

	LutRenderer lut_renderer;
	IrradianceRenderer irradiance_renderer;
	PrefilterRenderer prefilter_renderer;
	GBufferPass gbuffer_pass;
	ShadowRenderer shadow_renderer;

	SkyRenderer sky_renderer;
	DefferedLightingRenderer defferred_lighting_renderer;
	DefferedCompositeRenderer deffered_composite_renderer;
	PostProcessingRenderer post_renderer;
	DebugRenderer debug_renderer;

	SSAORenderer ssao_renderer;
	SSRRenderer ssr_renderer;
	DDGIRenderer ddgi_renderer;
	UpscaleRenderer upscale_renderer;

	PathTracingRenderer path_tracing_renderer;
};