#pragma once
#include "imgui.h"
#include "imgui/IconsFontAwesome6.h"

namespace UI
{
	void init();

	bool section(const char *label, bool default_open = false);

	void beginProperty(const char *label, const char *tooltip = nullptr);
	void endProperty();

	template <typename F>
	bool property(const char *label, F content, const char *tooltip = nullptr)
	{
		beginProperty(label, tooltip);
		bool changed = content();
		endProperty();
		return changed;
	}

	bool checkbox(const char *label, bool *value, const char *tooltip = nullptr);

	bool sliderFloat(const char *label, float *value, float min_value, float max_value, const char *format = "%.3f", bool logarithmic = false, const char *tooltip = nullptr);
	bool sliderInt(const char *label, int *value, int min_value, int max_value, const char *format = "%d", const char *tooltip = nullptr);

	bool dragFloat(const char *label, float *value, float speed, float min_value, float max_value, const char *format = "%.3f", bool logarithmic = false, const char *tooltip = nullptr);
	bool dragFloat3(const char *label, float *value, float speed, float min_value, float max_value, const char *format = "%.3f", bool logarithmic = false, const char *tooltip = nullptr);
	bool dragInt3(const char *label, int *value, float speed, int min_value, int max_value, const char *format = "%d", const char *tooltip = nullptr);

	bool inputFloat3(const char *label, float *value, const char *format = "%.3f", const char *tooltip = nullptr, ImGuiInputTextFlags flags = 0);
	bool inputInt(const char *label, int *value, const char *tooltip = nullptr, ImGuiInputTextFlags flags = 0);

	bool colorEdit3(const char *label, float *value, const char *tooltip = nullptr, ImGuiColorEditFlags flags = ImGuiColorEditFlags_Float);
	bool colorEdit4(const char *label, float *value, const char *tooltip = nullptr, ImGuiColorEditFlags flags = ImGuiColorEditFlags_Float);

	bool combo(const char *label, int *value, const char *const items[], int count, const char *tooltip = nullptr);
	bool radio(const char *label, int *value, const char *const items[], int count, const char *tooltip = nullptr);

	void text(const char *label, const char *format, ...);

	extern ImFont *font_regular;
	extern ImFont *font_bold;
	extern ImFont *font_small;
}