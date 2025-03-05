#include "pch.h"
#include "EditorApplication.h"
#include "Scene/Entity.h"
#include "Rendering/Renderer.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Filesystem.h"
#include "Editor/EditorDefaultScene.h"

#include "Variables.h"

#include "Editor/GuiUtils.h"

#include "FrameGraph/FrameGraph.h"
#include "FrameGraph/GraphViz.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"
#include "FrameGraph/FrameGraphUtils.h"
#include "Physics/PhysXWrapper.h"

using namespace physx;

static bool is_play = false;

EditorApplication::EditorApplication(int argc, char *argv[]) : Application(argc, argv)
{
	context.editor_camera = std::make_shared<Camera>();
	Renderer::setCamera(context.editor_camera);

	Scene::setCurrentScene(std::make_shared<Scene>());
	EditorDefaultScene::createScene();

	scene_renderer = std::make_shared<SceneRenderer>();

	debug_panel.sky_renderer = &scene_renderer->sky_renderer;
	debug_panel.defferred_lighting_renderer = &scene_renderer->defferred_lighting_renderer;
	debug_panel.post_renderer = &scene_renderer->post_renderer;
	debug_panel.debug_renderer = &scene_renderer->debug_renderer;
	debug_panel.ssao_renderer = &scene_renderer->ssao_renderer;
}

void EditorApplication::update(float delta_time)
{
	ImGui::ShowDemoWindow();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushFont(GuiUtils::roboto_regular_small);
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Scene"))
		{
			if (ImGui::MenuItem("Save"))
			{
				std::string path = Filesystem::saveFileDialog();
				if (!path.empty())
					Scene::getCurrentScene()->saveFile(path);
			}

			if (ImGui::MenuItem("Load"))
			{
				std::string path = Filesystem::openFileDialog();
				if (!path.empty())
				{
					Scene::setCurrentScene(std::make_shared<Scene>());
					Scene::getCurrentScene()->loadFile(path);
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	float menu_bar_height = ImGui::GetFrameHeight();
	ImGui::PopFont();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	//ImGui::ShowStyleEditor();
	//ImGui::ShowDemoWindow();

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos({viewport->Pos.x, viewport->Pos.y + menu_bar_height});
	ImGui::SetNextWindowSize({viewport->Size.x, viewport->Size.y - menu_bar_height});
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::Begin("DockSpace", nullptr, window_flags);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	ImGui::DockSpace(ImGui::GetID("EngineDockSpace"));

	// Viewport
	static bool prev_is_viewport_focused = false;
	bool is_viewport_focused = viewport_panel.renderImGui(context, delta_time);

	// TODO: move to DebugPanel
	// Debug Panel

	debug_panel.renderImGui(context);
	bool play_clicked = ImGui::Checkbox("Play", &is_play);

	ImGui::End();


	// Hierarchy
	hierarchy_panel.renderImGui(context);
	ImGui::End();

	// Parameters
	parameters_panel.renderImGui(context.selected_entity, scene_renderer->debug_renderer);

	// Asset browser
	asset_browser_panel.renderImGui();

	if (gInput.isKeyDown(GLFW_KEY_ESCAPE))
		context.selected_entity = Entity();

	viewport_panel.update();

	if (!ImGuizmo::IsUsing() && is_viewport_focused)
	{
		double mouse_x, mouse_y;
		glfwGetCursorPos(window, &mouse_x, &mouse_y);
		bool mouse_pressed = prev_is_viewport_focused != is_viewport_focused ? 0 : glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS;
		context.editor_camera->update(delta_time, glm::vec2(mouse_x, mouse_y), mouse_pressed);
	}
	context.editor_camera->updateMatrices();

	prev_is_viewport_focused = is_viewport_focused;

	static std::shared_ptr<Scene> saved_scene = nullptr;
	if (play_clicked)
	{
		context.selected_entity = Entity();
		if (is_play)
		{
			// Copy scene
			saved_scene = Scene::getCurrentScene();
			Scene::setCurrentScene(saved_scene->copy());
			Scene::getCurrentScene()->physics_scene->reinit();
		} else
		{
			// Revert scene
			Scene::setCurrentScene(saved_scene);
			saved_scene = nullptr;
		}
	}


	Scene::getCurrentScene()->physics_scene->draw_debug(&scene_renderer->debug_renderer);
	if (is_play)
	{
		Scene::getCurrentScene()->updateRuntime();
	}
}

void EditorApplication::updateBuffers(float delta_time)
{
	Renderer::updateDefaultUniforms(delta_time);
	scene_renderer->ssao_renderer.ubo_raw_pass.near_plane = context.editor_camera->getNear();
	scene_renderer->ssao_renderer.ubo_raw_pass.far_plane = context.editor_camera->getFar();
}


void EditorApplication::recordCommands(RHICommandList *cmd_list)
{
	scene_renderer->setScene(Scene::getCurrentScene());
	scene_renderer->render(context.editor_camera, viewport_panel.viewport_texture);

	FrameGraph frameGraph;

	// Render ImGui to backbuffer
	DefaultResourcesData &default_data = frameGraph.getBlackboard().add<DefaultResourcesData>();
	default_data.final = importTexture(frameGraph, viewport_panel.viewport_texture);
	default_data.backbuffer = importTexture(frameGraph, gDynamicRHI->getCurrentSwapchainTexture());

	default_data = frameGraph.addCallbackPass<DefaultResourcesData>("ImGui Pass",
	[&](RenderPassBuilder &builder, DefaultResourcesData &data)
	{
		data = default_data;
		data.final = builder.read(default_data.final);
		data.backbuffer = builder.write(default_data.backbuffer, RenderPassNode::RESOURCE_ACCESS_IGNORE_FLAG);
		builder.setSideEffect(true);
	},
	[=](const DefaultResourcesData &data, const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto &final = resources.getResource<FrameGraphTexture>(data.final);
		auto &backbuffer = resources.getResource<FrameGraphTexture>(data.backbuffer);

		cmd_list->setRenderTargets({backbuffer.texture}, {}, 0, 0, true);
		ImGuiWrapper::render(cmd_list);
		cmd_list->resetRenderTargets();
	});

	frameGraph.compile();

	auto current_cmd_list = gDynamicRHI->getCmdList();
	frameGraph.execute(current_cmd_list);
}

void EditorApplication::cleanupResources()
{
	/*
	shadow_renderer.ray_tracing_scene = nullptr;
	ray_tracing_scene = nullptr;
	*/
}
