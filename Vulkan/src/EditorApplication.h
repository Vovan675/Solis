#pragma once
#include "RHI/DynamicRHI.h"
#include "Log.h"
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
#include "Editor/ViewportPanel.h"
#include "Editor/AssetBrowserPanel.h"
#include "Editor/ParametersPanel.h"
#include "Editor/HierarchyPanel.h"
#include "Editor/DebugPanel.h"
#include "Application.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "RHI/RayTracing/RayTracingScene.h"
#include "ConsoleVariables.h"
#include "FrameGraph/FrameGraph.h"
#include "Rendering/SceneRenderer.h"

class EditorApplication : public Application
{
public:
	EditorApplication(int argc, char *argv[]);
protected:
	void update(float delta_time) override;
	void updateBuffers(float delta_time) override;
	void recordCommands(RHICommandList *cmd_list) override;
	void cleanupResources() override;
private:
	Ref<RayTracingScene> ray_tracing_scene;

	EditorContext context;

	ViewportPanel viewport_panel;
	AssetBrowserPanel asset_browser_panel;
	ParametersPanel parameters_panel;
	HierarchyPanel hierarchy_panel;
	DebugPanel debug_panel;

	Ref<SceneRenderer> scene_renderer;
};