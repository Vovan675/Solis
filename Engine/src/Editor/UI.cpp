#include "pch.h"
#include "UI.h"

ImFont *UI::font_regular;
ImFont *UI::font_small;
ImFont *UI::font_bold;

void UI::init()
{
	ImGui::StyleColorsDark();

	ImGuiStyle &style = ImGui::GetStyle();

	ImVec4 *colors = style.Colors;
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
	style.WindowPadding = ImVec2(8, 8);
	style.FramePadding = ImVec2(2, 2);
	style.ItemSpacing = ImVec2(4, 4);
	style.ItemInnerSpacing = ImVec2(4, 4);
	style.IndentSpacing = 18;
	style.ScrollbarSize = 12;
	style.GrabMinSize = 12;
	style.WindowRounding = 4;
	style.ChildRounding = 4;
	style.FrameRounding = 4;
	style.PopupRounding = 4;
	style.ScrollbarRounding = 4;
	style.GrabRounding = 4;
	style.TabRounding = 0;
	style.SeparatorTextBorderSize = 1;
	style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
	style.SeparatorTextPadding = ImVec2(16, 3);

	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImFontConfig config { };
	config.MergeMode = true;

	ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

	font_regular = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", 18);
	io.Fonts->AddFontFromFileTTF("assets/fonts/fa-solid-900.ttf", 18, &config, icon_ranges);

	font_bold = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Medium.ttf", 18);
	io.Fonts->AddFontFromFileTTF("assets/fonts/fa-solid-900.ttf", 18, &config, icon_ranges);

	font_small = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", 14);
	io.Fonts->AddFontFromFileTTF("assets/fonts/fa-solid-900.ttf", 14, &config, icon_ranges);
	io.FontDefault = font_regular;
	io.Fonts->Build();
}

bool UI::section(const char *label, bool default_open)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
	if (default_open)
		flags |= ImGuiTreeNodeFlags_DefaultOpen;

	ImGui::PushFont(font_bold);
	bool open = ImGui::CollapsingHeader(label, flags);
	ImGui::PopFont();
	return open;
}

static const float property_label_fraction = 0.45f;
static const float property_label_min_width = 110.0f;
static const char *property_tooltip = nullptr;

void UI::beginProperty(const char *label, const char *tooltip)
{
	float label_width = std::max(ImGui::GetContentRegionAvail().x * property_label_fraction, property_label_min_width);
	float row_start_x = ImGui::GetCursorPosX();

	property_tooltip = tooltip;
	ImGui::PushID(label);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	if (tooltip)
		ImGui::SetItemTooltip("%s", tooltip);

	ImGui::SameLine(row_start_x + label_width);
	ImGui::SetNextItemWidth(-FLT_MIN);
}

void UI::endProperty()
{
	if (property_tooltip)
		ImGui::SetItemTooltip("%s", property_tooltip);
	property_tooltip = nullptr;
	ImGui::PopID();
}

bool UI::checkbox(const char *label, bool *value, const char *tooltip)
{
	return property(label, [&] { return ImGui::Checkbox("##value", value); }, tooltip);
}

bool UI::sliderFloat(const char *label, float *value, float min_value, float max_value, const char *format, bool logarithmic, const char *tooltip)
{
	ImGuiSliderFlags flags = logarithmic ? ImGuiSliderFlags_Logarithmic : 0;
	return property(label, [&] { return ImGui::SliderFloat("##value", value, min_value, max_value, format, flags); }, tooltip);
}

bool UI::sliderInt(const char *label, int *value, int min_value, int max_value, const char *format, const char *tooltip)
{
	return property(label, [&] { return ImGui::SliderInt("##value", value, min_value, max_value, format); }, tooltip);
}

bool UI::dragFloat(const char *label, float *value, float speed, float min_value, float max_value, const char *format, bool logarithmic, const char *tooltip)
{
	ImGuiSliderFlags flags = logarithmic ? ImGuiSliderFlags_Logarithmic : 0;
	return property(label, [&] { return ImGui::DragFloat("##value", value, speed, min_value, max_value, format, flags); }, tooltip);
}

bool UI::dragFloat3(const char *label, float *value, float speed, float min_value, float max_value, const char *format, bool logarithmic, const char *tooltip)
{
	ImGuiSliderFlags flags = logarithmic ? ImGuiSliderFlags_Logarithmic : 0;
	return property(label, [&] { return ImGui::DragFloat3("##value", value, speed, min_value, max_value, format, flags); }, tooltip);
}

bool UI::dragInt3(const char *label, int *value, float speed, int min_value, int max_value, const char *format, const char *tooltip)
{
	return property(label, [&] { return ImGui::DragInt3("##value", value, speed, min_value, max_value, format); }, tooltip);
}

bool UI::inputFloat3(const char *label, float *value, const char *format, const char *tooltip, ImGuiInputTextFlags flags)
{
	return property(label, [&] { return ImGui::InputFloat3("##value", value, format, flags); }, tooltip);
}

bool UI::inputInt(const char *label, int *value, const char *tooltip, ImGuiInputTextFlags flags)
{
	return property(label, [&] { return ImGui::InputInt("##value", value, 1, 100, flags); }, tooltip);
}

bool UI::colorEdit3(const char *label, float *value, const char *tooltip, ImGuiColorEditFlags flags)
{
	return property(label, [&] { return ImGui::ColorEdit3("##value", value, flags); }, tooltip);
}

bool UI::colorEdit4(const char *label, float *value, const char *tooltip, ImGuiColorEditFlags flags)
{
	return property(label, [&] { return ImGui::ColorEdit4("##value", value, flags); }, tooltip);
}

bool UI::combo(const char *label, int *value, const char *const items[], int count, const char *tooltip)
{
	return property(label, [&] { return ImGui::Combo("##value", value, items, count); }, tooltip);
}

bool UI::radio(const char *label, int *value, const char *const items[], int count, const char *tooltip)
{
	return property(label, [&]
	{
		bool changed = false;
		for (int i = 0; i < count; i++)
		{
			if (i > 0)
				ImGui::SameLine();
			changed |= ImGui::RadioButton(items[i], value, i);
		}
		return changed;
	}, tooltip);
}

void UI::text(const char *label, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	property(label, [&] { ImGui::TextV(format, args); return false; });
	va_end(args);
}
