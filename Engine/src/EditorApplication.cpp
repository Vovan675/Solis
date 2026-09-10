#include "pch.h"
#include "EditorApplication.h"
#include "Assets/AssetManager.h"
#include "Scene/Entity.h"
#include "Rendering/Renderer.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Core/Filesystem.h"
#include "Core/Platform.h"
#include "Editor/EditorDefaultScene.h"

#include "Core/Variables.h"

#include "Editor/UI.h"

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

}

static const eastl::string scenes_directory = "assets/scenes/";

static ImVec2 calc_item_center()
{
	ImVec2 min = ImGui::GetItemRectMin();
	ImVec2 max = ImGui::GetItemRectMax();
	return ImVec2(floor((min.x + max.x) * 0.5f) + 0.5f, floor((min.y + max.y) * 0.5f) + 0.5f);
}

static void draw_rect_no_aa(ImDrawList *draw_list, ImVec2 min, ImVec2 max, ImU32 color)
{
	draw_list->AddRectFilled(min, ImVec2(max.x, min.y + 1), color);
	draw_list->AddRectFilled(ImVec2(min.x, max.y - 1), max, color);
	draw_list->AddRectFilled(min, ImVec2(min.x + 1, max.y), color);
	draw_list->AddRectFilled(ImVec2(max.x - 1, min.y), max, color);
}

static bool window_button(const char *id, ImVec2 size, ImU32 hover_color)
{
	bool pressed = ImGui::InvisibleButton(id, size);
	if (ImGui::IsItemActive())
		hover_color = ImGui::GetColorU32(ImGuiCol_ButtonActive);
	if (ImGui::IsItemHovered())
		ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), hover_color);
	return pressed;
}

static bool tool_button(const char *label, ImVec4 color = ImVec4(1, 1, 1, 1))
{
	ImVec2 button_size(24, 24);
	ImGui::SetCursorPosY((ImGui::GetWindowHeight() - button_size.y) * 0.5f); // center
	ImGui::PushStyleColor(ImGuiCol_Text, color);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	bool pressed = ImGui::Button(label, button_size);
	ImGui::PopStyleColor(2);
	return pressed;
}

float EditorApplication::draw_top_bar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 9));
	ImGui::PushFont(UI::font_regular);

	float height = ImGui::GetFrameHeight();

	if (ImGui::BeginMainMenuBar())
	{
		ImGui::SetCursorPosX(12);

		ImGui::PushFont(UI::font_bold);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.35f, 0.22f, 1.0f));
		ImGui::TextUnformatted(ICON_FA_SUN "  SOLIS");
		ImGui::PopStyleColor();
		ImGui::PopFont();

		ImGui::SameLine(0, 24);

		if (ImGui::BeginMenu("Scene"))
		{
			for (const auto &entry : std::filesystem::directory_iterator(scenes_directory.c_str()))
			{
				if (entry.path().extension() != ".scene")
					continue;

				if (ImGui::MenuItem(entry.path().stem().string().c_str()))
					openScene(entry.path().string().c_str());
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Options"))
		{
			ImGui::MenuItem("Auto Refresh Shaders", nullptr, &auto_refresh_shaders);
			ImGui::EndMenu();
		}

		ImGui::SameLine(0, 24);

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 3));

		if (tool_button(ICON_FA_FLOPPY_DISK, ImVec4(0.2f, 0.4f, 0.5f, 1.0f)))
		{
			eastl::string path = Filesystem::saveFileDialog();
			if (!path.empty())
				Scene::getCurrentScene()->saveFile(path);
		}

		if (tool_button(ICON_FA_FOLDER_OPEN, ImVec4(0.6f, 0.5f, 0.4f, 1.0f)))
		{
			eastl::string path = Filesystem::openFileDialog();
			if (!path.empty())
				openScene(path);
		}

		ImGui::SameLine(0, 24);

		if (tool_button(is_play ? ICON_FA_STOP : ICON_FA_PLAY, is_play ? ImVec4(0.73f, 0.32f, 0.32f, 1.0f) : ImVec4(0.35f, 0.57f, 0.35f, 1.0f)))
			toggle_play();

		ImGui::PopStyleVar();

		// Default window buttons
		ImVec2 button_size(46, height);
		float drag_min_x = ImGui::GetCursorPosX();
		float drag_max_x = floor(ImGui::GetWindowWidth()) - button_size.x * 3;
		ImGui::SetCursorPos(ImVec2(drag_max_x, 0));

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		ImDrawList *draw_list = ImGui::GetWindowDrawList();
		ImU32 icon_color = ImGui::GetColorU32(ImGuiCol_Text);
		ImU32 hover_color = ImGui::GetColorU32(ImGuiCol_ButtonHovered);

		if (window_button("##minimize", button_size, hover_color))
			glfwIconifyWindow(window);
		ImVec2 center = calc_item_center();
		draw_list->AddRectFilled(ImVec2(center.x - 5, center.y), ImVec2(center.x + 5, center.y + 1), icon_color);

		window_button("##maximize", button_size, hover_color);
		Platform::setMaximizeButtonRect(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y, ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y);
		center = calc_item_center();
		if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED))
		{
			draw_rect_no_aa(draw_list, ImVec2(center.x - 6, center.y - 2), ImVec2(center.x + 2, center.y + 6), icon_color);
			draw_list->AddRectFilled(ImVec2(center.x - 2, center.y - 6), ImVec2(center.x + 6, center.y - 5), icon_color);
			draw_list->AddRectFilled(ImVec2(center.x + 5, center.y - 6), ImVec2(center.x + 6, center.y + 2), icon_color);
		} else
		{
			draw_rect_no_aa(draw_list, ImVec2(center.x - 5, center.y - 5), ImVec2(center.x + 5, center.y + 5), icon_color);
		}

		if (window_button("##close", button_size, IM_COL32(196, 42, 42, 255)))
			glfwSetWindowShouldClose(window, true);
		center = calc_item_center();
		draw_list->AddLine(ImVec2(center.x - 5, center.y - 5), ImVec2(center.x + 5, center.y + 5), icon_color);
		draw_list->AddLine(ImVec2(center.x + 5, center.y - 5), ImVec2(center.x - 5, center.y + 5), icon_color);

		ImGui::PopStyleVar();

		Platform::setTitleBarDragRect(drag_min_x, drag_max_x, height);

		ImGui::EndMainMenuBar();
	}

	ImGui::PopFont();
	ImGui::PopStyleVar(2);

	return height;
}

void EditorApplication::toggle_play()
{
	static Ref<Scene> saved_scene = nullptr;

	is_play = !is_play;
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

void EditorApplication::init()
{
	shaders_watcher.addPath(L"shaders", true);

	context.editor_camera = Camera(glm::vec3(0, 2, 0));
	Renderer::setCamera(&context.editor_camera);
	EditorContext::current = &context;

	scene_renderer = new SceneRenderer();

	debug_panel.debug_renderer = &scene_renderer->debug_renderer;
	debug_panel.geometry_streaming = &scene_renderer->geometry_streaming;
	debug_panel.mitsuba_bridge = &mitsuba_bridge;

	asset_browser_panel.init();

	openScene("assets/scenes/" + engine_startup_scene.get() + ".scene");
}

void EditorApplication::openScene(const eastl::string &path)
{
	context.selected_entity = Entity();
	context.selected_entities.clear();
	context.selected_path.clear();

	if (std::filesystem::exists(path.c_str()))
		Scene::loadScene(path);
	else
		EditorDefaultScene::createScene(&context.editor_camera);
}

void EditorApplication::update(float delta_time)
{
	//ImGui::ShowDemoWindow();
	bool is_window_focused = glfwGetWindowAttrib(window, GLFW_FOCUSED);
	if (is_window_focused && !was_window_focused)
	{
		AssetManager::refresh();
		asset_browser_panel.refreshCache();
	}
	was_window_focused = is_window_focused;

	if (auto_refresh_shaders)
	{
		shaders_watcher.checkUpdates([](eastl::wstring path)
		{
			auto all_shaders = RHIShader::getAllShadersAtPath(path);
			for (RHIShader *shader : all_shaders)
			{
				shader->recompile();
			}
			CORE_INFO("Shader recompiled {}", std::filesystem::path(path.c_str()).string());
		});
	}

	float top_bar_height = draw_top_bar();

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos({viewport->Pos.x, viewport->Pos.y + top_bar_height});
	ImGui::SetNextWindowSize({viewport->Size.x, viewport->Size.y - top_bar_height});
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	window_flags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::Begin("DockSpace", nullptr, window_flags);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	ImGui::DockSpace(ImGui::GetID("EngineDockSpace"));

	ImGui::End();

	// Common
	// Add rect over full window (resize bar)
	if (!glfwGetWindowAttrib(window, GLFW_MAXIMIZED))
		ImGui::GetForegroundDrawList()->AddRect(viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), IM_COL32(70, 70, 70, 255));

	// Viewport
	static bool prev_is_viewport_focused = false;
	bool is_viewport_focused = viewport_panel.renderImGui(context, delta_time);

	render_settings_panel.renderImGui(context);
	debug_panel.renderImGui(context);

	// Hierarchy
	hierarchy_panel.renderImGui(context);
	// Parameters
	parameters_panel.renderImGui(context, scene_renderer->debug_renderer, asset_browser_panel);

	// Asset browser
	asset_browser_panel.renderImGui(context);

	if (gInput.isKeyDown(GLFW_KEY_ESCAPE))
	{
		context.selected_entity = Entity();
		context.selected_path.clear();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_M, false) && !ImGui::GetIO().WantTextInput)
		mitsuba_bridge.runRender(context);

	viewport_panel.update();

	if (!ImGuizmo::IsUsing() && is_viewport_focused)
	{
		double mouse_x, mouse_y;
		glfwGetCursorPos(window, &mouse_x, &mouse_y);
		bool mouse_pressed = prev_is_viewport_focused != is_viewport_focused ? 0 : glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS;
		context.editor_camera.update(delta_time, glm::vec2(mouse_x, mouse_y), mouse_pressed);
	}
	context.editor_camera.updateMatrices();

	prev_is_viewport_focused = is_viewport_focused;

	Scene::getCurrentScene()->physics_scene->draw_debug(&scene_renderer->debug_renderer);
	if (is_play)
	{
		Scene::getCurrentScene()->updateRuntime();
	}
}

void EditorApplication::updateBuffers(float delta_time)
{
	Renderer::updateDefaultUniforms(delta_time);
}


void EditorApplication::recordCommands(RHICommandList *cmd_list)
{
	scene_renderer->setScene(Scene::getCurrentScene());
	scene_renderer->render(&context.editor_camera, viewport_panel.viewport_texture);

	FrameGraph frameGraph;

	// Render ImGui to backbuffer
	frameGraph.importTexture(GFXRID(FinalTexture), viewport_panel.viewport_texture);
	frameGraph.importTexture(GFXRID(BackbufferTexture), gDynamicRHI->getCurrentSwapchainTexture());

	frameGraph.addCallbackPass("ImGui Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.readTexture(GFXRID(FinalTexture));
		builder.writeTexture(GFXRID(BackbufferTexture));
		builder.setSideEffect(true);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		auto final = resources.getTexture(GFXRID(FinalTexture));
		auto backbuffer = resources.getTexture(GFXRID(BackbufferTexture));

		cmd_list->setRenderTargets({backbuffer}, {}, 0, 0, true);
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

	scene_renderer = nullptr;
	viewport_panel.viewport_texture = nullptr;
	asset_browser_panel = {};
	mitsuba_bridge = {};
}
