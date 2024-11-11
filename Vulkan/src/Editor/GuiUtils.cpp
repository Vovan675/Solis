#include "pch.h"
#include "GuiUtils.h"
#include "imgui.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/ImGuiWrapper.h"
#include "Rendering/Renderer.h"

ImFont *GuiUtils::roboto_regular_medium;
ImFont *GuiUtils::roboto_regular_small;

void GuiUtils::init()
{
	ImGui::StyleColorsDark();

	ImGuiStyle &style = ImGui::GetStyle();

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_FrameBg]                = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
	colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
	colors[ImGuiCol_FrameBgActive]          = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
	colors[ImGuiCol_MenuBarBg]              = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
	colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
	colors[ImGuiCol_CheckMark]              = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
	colors[ImGuiCol_SliderGrab]             = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
	colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
	colors[ImGuiCol_Button]                 = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_ButtonHovered]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ButtonActive]           = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
	colors[ImGuiCol_Header]                 = ImVec4(0.30f, 0.30f, 0.30f, 0.31f);
	colors[ImGuiCol_HeaderHovered]          = ImVec4(0.41f, 0.41f, 0.41f, 0.80f);
	colors[ImGuiCol_HeaderActive]           = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	colors[ImGuiCol_Separator]              = ImVec4(0.27f, 0.27f, 0.27f, 0.50f);
	colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.35f, 0.35f, 0.35f, 0.78f);
	colors[ImGuiCol_SeparatorActive]        = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
	colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
	colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
	colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
	colors[ImGuiCol_Tab]                    = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
	colors[ImGuiCol_TabHovered]             = ImVec4(0.38f, 0.38f, 0.38f, 0.80f);
	colors[ImGuiCol_TabActive]              = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
	colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_DockingPreview]         = ImVec4(0.31f, 0.31f, 0.31f, 0.70f);
	colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);

	colors[ImGuiCol_WindowBg]               = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_ChildBg]                = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);

	style.WindowMenuButtonPosition = ImGuiDir_None;
	style.WindowRounding = 4;
	style.FrameRounding = 4;
	style.ScrollbarRounding = 4;
	style.GrabRounding = 4;
	style.TabRounding = 4;

	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	roboto_regular_medium = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", 18);
	ImFontConfig config { };
	config.MergeMode = true;

	ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
	auto icons = io.Fonts->AddFontFromFileTTF("assets/fonts/fa-solid-900.ttf", 14, &config, icon_ranges);
	roboto_regular_small = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", 14);
	io.FontDefault = roboto_regular_medium;
	io.Fonts->Build();
}



void GuiUtils::draw_stats_bar(float delta_time, ImVec2 pos)
{
	ImVec4 color_black(22.0f / 255.0f, 22.0f / 255.0f, 22.0f / 255.0f, 0.5);
	ImVec4 color_white(190.0f / 255.0f, 190.0f / 255.0f, 190.0f / 255.0f, 1.0);
	ImGui::SetNextWindowPos(pos);
	ImGui::Begin("Stats Bar", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove);

	ImGui::PushFont(roboto_regular_small);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 5));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, color_black);

	// FPS
	ImGui::BeginChild("FPS", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);
	static float last_fps = 0;
	static float last_fps_timer = 0.3f;
	last_fps_timer -= delta_time;

	if (last_fps_timer <= 0)
	{
		last_fps_timer = 0.3;
		last_fps = 1.0f / delta_time;
	}

	ImGui::Text("FPS: %i (%f ms)", (int)(last_fps), 1.0f / last_fps * 1000);
	
	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
	ImGui::PopFont();

	ImGui::End();
}
