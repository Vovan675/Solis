#pragma once
#include "imgui.h"
#include "imgui/IconsFontAwesome6.h"
#include "Core/Reflection.h"
#include "Core/ConsoleVariables.h"

namespace UI
{
	void init();

	bool beginSection(const char *label, bool default_open = false);
	void endSection();

	bool beginProperty(const char *label, const char *tooltip = nullptr, bool can_expand = false);
	void endProperty(bool is_open = false);

	template <typename F>
	bool property(const char *label, F content, const char *tooltip = nullptr)
	{
		beginProperty(label, tooltip);
		bool changed = content();
		if (tooltip)
			ImGui::SetItemTooltip("%s", tooltip);
		endProperty();
		return changed;
	}

	void text(const char *label, const char *format, ...);

	bool checkbox(const char *label, bool *value, const char *tooltip = nullptr);

	bool sliderFloat(const char *label, float *value, float min_value, float max_value, const char *format = "%.3f", bool logarithmic = false, const char *tooltip = nullptr);
	bool sliderInt(const char *label, int *value, int min_value, int max_value, const char *format = "%d", bool logarithmic = false, const char *tooltip = nullptr);

	bool dragFloat(const char *label, float *value, float min_value, float max_value, const char *format = "%.3f", bool logarithmic = false, const char *tooltip = nullptr);
	bool dragFloat3(const char *label, float *value, float min_value, float max_value, const char *format = "%.3f", bool logarithmic = false, const char *tooltip = nullptr);
	bool dragFloat3(const char *label, float *value, float speed, const char *tooltip = nullptr);
	bool dragFloat4(const char *label, float *value, float min_value, float max_value, const char *format = "%.3f", bool logarithmic = false, const char *tooltip = nullptr);
	bool dragInt3(const char *label, int *value, int min_value, int max_value, const char *format = "%d", bool logarithmic = false, const char *tooltip = nullptr);

	bool inputFloat3(const char *label, float *value, const char *tooltip = nullptr, ImGuiInputTextFlags flags = 0);
	bool inputInt(const char *label, int *value, const char *tooltip = nullptr, ImGuiInputTextFlags flags = 0);
	bool inputInt2(const char *label, int *value, const char *tooltip = nullptr, ImGuiInputTextFlags flags = 0);

	bool colorEdit3(const char *label, float *value, const char *tooltip = nullptr, ImGuiColorEditFlags flags = ImGuiColorEditFlags_Float);
	bool colorEdit4(const char *label, float *value, const char *tooltip = nullptr, ImGuiColorEditFlags flags = ImGuiColorEditFlags_Float);

	bool combo(const char *label, int *value, const char *const items[], int count, const char *tooltip = nullptr);
	bool radio(const char *label, int *value, const char *const items[], int count, const char *tooltip = nullptr);

	bool inputText(const char *label, eastl::string *value, const char *tooltip = nullptr);

	bool convar(ConVarDescription *description);

	bool assetField(const char *label, AssetReference *reference, const AssetTypeInfo *accepted_type, const char *tooltip = nullptr);

	using PropertyDrawer = bool (*)(const FieldInfo &field, const char *label, void *value, void *owner);

	bool addDrawer(const void *key, PropertyDrawer drawer);

	template<typename T>
	bool registerTypeDrawer(PropertyDrawer drawer)
	{
		if constexpr (Reflected<T>::isReflected)
			return addDrawer(&Reflected<T>::getInfo(), drawer);
		else
			return addDrawer(&ValueInfo::get<T>(), drawer);
	}

	template<typename T>
	bool registerFieldDrawer(const char *field_name, PropertyDrawer drawer)
	{
		return addDrawer(Reflected<T>::getInfo().findField(field_name), drawer);
	}

	bool drawStruct(const StructInfo &info, void *object);

	template <typename T>
	bool drawStruct(T &object)
	{
		return drawStruct(Reflected<T>::getInfo(), &object);
	}

	bool drawAsset(const AssetTypeInfo &type, const std::filesystem::path &path);

	extern ImFont *font_regular;
	extern ImFont *font_bold;
	extern ImFont *font_small;

	constexpr ImVec4 background_color = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	constexpr ImVec4 background_dark_color = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
	constexpr ImVec4 border_color = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
	constexpr ImVec4 accent_color = ImVec4(0.96f, 0.35f, 0.22f, 1.00f);
	constexpr ImVec4 selection_color = ImVec4(0.22f, 0.31f, 0.53f, 1.00f);
	constexpr ImVec4 success_color = ImVec4(0.35f, 0.57f, 0.35f, 1.00f);
	constexpr ImVec4 warning_color = ImVec4(0.80f, 0.40f, 0.00f, 1.00f);
	constexpr ImVec4 error_color = ImVec4(0.90f, 0.35f, 0.30f, 1.00f);
}
