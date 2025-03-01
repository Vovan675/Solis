#pragma once
#include "RHI/DynamicRHI.h"
#include "Mesh.h"
#include "Scene/Scene.h"
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


class SceneRenderer
{
public:
	SceneRenderer();

	void setScene(std::shared_ptr<Scene> scene) { this->scene = scene; }
	void render(std::shared_ptr<Camera> camera, std::shared_ptr<RHITexture> result_texture);

public:
	friend class EditorApplication;

	std::shared_ptr<Scene> scene;

	std::vector<RenderBatch> render_batches;

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