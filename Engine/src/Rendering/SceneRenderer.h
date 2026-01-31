#pragma once
#include "RHI/DynamicRHI.h"
#include "Rendering/Mesh.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "renderers/EntityRenderer.h"
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

#include "renderers/PathTracingRenderer.h"
#include "ShaderStructs.h"

class SceneRenderer : public RefCounted
{
public:
	SceneRenderer();

	void setScene(Ref<Scene> scene);
	void render(Camera *camera, RHITextureRef result_texture);

	Ref<RayTracingScene> getCurrentRayTracingScene() const { return rt_scene; }
public:
	friend class EditorApplication;

	void render_deferred(Camera *camera, FrameGraph &frame_graph);
	void render_path_traced(Camera *camera, FrameGraph &frame_graph);
	void update(Camera *camera);

	void gpu_frame_cull(FrameGraph &frame_graph);

	Camera *main_view_camera = nullptr;

	Ref<Scene> scene;
	Ref<RayTracingScene> rt_scene;

	eastl::vector<FrustumDataGPU> frustums;
	eastl::vector<MaterialGPU> materials;
	eastl::vector<MeshGPU> meshes;
	eastl::vector<InstanceGPU> instances; // All that passed cpu coarse culling and can be rendered
	eastl::vector<uint32_t> instances_pass_masks; // Instance pass mask, every bit indicates at which pass (frustum) this instance is visible

	RHIBufferRef frustums_gpu;
	RHIBufferRef materials_gpu;
	RHIBufferRef meshes_gpu;
	RHIBufferRef instances_gpu;
	RHIBufferRef instances_pass_masks_gpu;

	uint32_t indirect_draw_calls_max_count;

	EntityRenderer entity_renderer;

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

	PathTracingRenderer path_tracing_renderer;
};