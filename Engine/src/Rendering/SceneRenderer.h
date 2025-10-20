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


class SceneRenderer : public RefCounted
{
public:
	SceneRenderer();

	void setScene(Ref<Scene> scene);
	void render(Camera *camera, RHITextureRef result_texture);

	Ref<RayTracingScene> getCurrentRayTracingScene() const { return rt_scene; }
public:
	friend class EditorApplication;

	void update(Camera *camera);

	Ref<Scene> scene;
	Ref<RayTracingScene> rt_scene;

	eastl::vector<RenderBatch> render_batches;
	RHIBufferRef materials_buffer;
	RHIBufferRef instances_buffer;

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
};