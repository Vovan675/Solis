#pragma once
#include "RHI/Buffer.h"
#include "RHI/Device.h"
#include "Log.h"
#include "Mesh.h"
#include "Scene/Scene.h"
#include "RHI/Pipeline.h"
#include "RHI/Shader.h"
#include "RHI/Swapchain.h"
#include "RHI/Texture.h"
#include "RHI/VkWrapper.h"
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

class EditorApplication : public Application
{
public:
	EditorApplication();
protected:
	void update(float delta_time) override;
	void updateBuffers(float delta_time) override;
	void recordCommands(CommandBuffer &command_buffer) override;
	void cleanupResources() override;
private:
	std::shared_ptr<RayTracingScene> ray_tracing_scene;

	EntityRenderer entity_renderer;

	EditorContext context;

	ViewportPanel viewport_panel;
	AssetBrowserPanel asset_browser_panel;
	ParametersPanel parameters_panel;
	HierarchyPanel hierarchy_panel;
	DebugPanel debug_panel;

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