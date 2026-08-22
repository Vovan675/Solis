#include "pch.h"
#include "UI.h"
#include "EditorContext.h"
#include "Assets/AssetManager.h"
#include "Core/Filesystem.h"

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

bool UI::beginSection(const char *label, bool default_open)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	if (default_open)
		flags |= ImGuiTreeNodeFlags_DefaultOpen;

	ImGui::PushFont(font_bold);
	bool is_open = ImGui::TreeNodeEx(label, flags);
	ImGui::PopFont();

	if (is_open)
		ImGui::Indent();
	return is_open;
}

void UI::endSection()
{
	ImGui::Unindent();
}

static const float property_label_fraction = 0.45f;
static const float property_label_min_width = 110.0f;

bool UI::beginProperty(const char *label, const char *tooltip, bool can_expand)
{
	float label_width = std::max(ImGui::GetContentRegionAvail().x * property_label_fraction, property_label_min_width);
	float value_start_x = ImGui::GetCursorPosX() + label_width;

	ImGui::PushID(label);
	ImGui::AlignTextToFramePadding();

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_AllowOverlap;
	if (!can_expand)
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

	bool is_open = ImGui::TreeNodeEx("##label", flags, "%s", label) && can_expand;
	if (tooltip)
		ImGui::SetItemTooltip("%s", tooltip);

	ImGui::SameLine(value_start_x);
	ImGui::SetNextItemWidth(-FLT_MIN);
	return is_open;
}

void UI::endProperty(bool is_open)
{
	if (is_open)
		ImGui::TreePop();
	ImGui::PopID();
}

void UI::text(const char *label, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	property(label, [&] { ImGui::TextV(format, args); return false; });
	va_end(args);
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

bool UI::sliderInt(const char *label, int *value, int min_value, int max_value, const char *format, bool logarithmic, const char *tooltip)
{
	ImGuiSliderFlags flags = logarithmic ? ImGuiSliderFlags_Logarithmic : 0;
	return property(label, [&] { return ImGui::SliderInt("##value", value, min_value, max_value, format, flags); }, tooltip);
}

static float calc_drag_speed(float min_value, float max_value)
{
	return min_value != max_value ? (max_value - min_value) / 100.0f : 1.0f;
}

bool UI::dragFloat(const char *label, float *value, float min_value, float max_value, const char *format, bool logarithmic, const char *tooltip)
{
	ImGuiSliderFlags flags = logarithmic ? ImGuiSliderFlags_Logarithmic : 0;
	float speed = calc_drag_speed(min_value, max_value);
	return property(label, [&] { return ImGui::DragFloat("##value", value, speed, min_value, max_value, format, flags); }, tooltip);
}

bool UI::dragFloat3(const char *label, float *value, float min_value, float max_value, const char *format, bool logarithmic, const char *tooltip)
{
	ImGuiSliderFlags flags = logarithmic ? ImGuiSliderFlags_Logarithmic : 0;
	float speed = calc_drag_speed(min_value, max_value);
	return property(label, [&] { return ImGui::DragFloat3("##value", value, speed, min_value, max_value, format, flags); }, tooltip);
}

bool UI::dragFloat4(const char *label, float *value, float min_value, float max_value, const char *format, bool logarithmic, const char *tooltip)
{
	ImGuiSliderFlags flags = logarithmic ? ImGuiSliderFlags_Logarithmic : 0;
	float speed = calc_drag_speed(min_value, max_value);
	return property(label, [&] { return ImGui::DragFloat4("##value", value, speed, min_value, max_value, format, flags); }, tooltip);
}

bool UI::dragInt3(const char *label, int *value, int min_value, int max_value, const char *format, bool logarithmic, const char *tooltip)
{
	ImGuiSliderFlags flags = logarithmic ? ImGuiSliderFlags_Logarithmic : 0;
	float speed = std::max(1.0f, calc_drag_speed(min_value, max_value));
	return property(label, [&] { return ImGui::DragInt3("##value", value, speed, min_value, max_value, format, flags); }, tooltip);
}

bool UI::inputFloat3(const char *label, float *value, const char *tooltip, ImGuiInputTextFlags flags)
{
	return property(label, [&] { return ImGui::InputFloat3("##value", value, "%.3f", flags); }, tooltip);
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

static int imgui_input_text_callback(ImGuiInputTextCallbackData *data)
{
	eastl::string *value = (eastl::string *)data->UserData;
	value->resize(data->BufTextLen);
	data->Buf = value->data();
	return 0;
}

bool UI::inputText(const char *label, eastl::string *value, const char *tooltip)
{
	return property(label, [&]
	{
		return ImGui::InputText("##value", value->data(), value->capacity() + 1, ImGuiInputTextFlags_CallbackResize, imgui_input_text_callback, value);
	}, tooltip);
}

bool UI::convar(ConVarDescription *description)
{
	if (description->type == CON_VAR_TYPE_INT)
		return inputInt(description->label, &ConVarSystem::getCVar<int>(description->index).current_value, description->name);
	if (description->type == CON_VAR_TYPE_FLOAT)
		return dragFloat(description->label, &ConVarSystem::getCVar<float>(description->index).current_value, 0.0f, 0.0f, "%.3f", false, description->name);
	if (description->type == CON_VAR_TYPE_STRING)
		return inputText(description->label, &ConVarSystem::getCVar<eastl::string>(description->index).current_value, description->name);
	return checkbox(description->label, &ConVarSystem::getCVar<bool>(description->index).current_value, description->name);
}

static bool is_asset_of_type(const std::filesystem::path &path, const AssetTypeInfo *accepted_type)
{
	return AssetManager::findTypeInfoByExtension(path.extension().string().c_str()) == accepted_type;
}

static void draw_asset_search_picker(std::filesystem::path *path, const AssetTypeInfo *asset_type)
{
	static char search_buf[64];
	if (ImGui::IsWindowAppearing())
	{
		search_buf[0] = 0;
		ImGui::SetKeyboardFocusHere();
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##search_buf", ICON_FA_MAGNIFYING_GLASS " Search", search_buf, sizeof(search_buf));

	if (ImGui::Selectable("None", path->empty()))
	{
		path->clear();
		return;
	}

	eastl::string search = search_buf;
	search.make_lower();

	eastl::vector<const std::filesystem::path *> matched_paths;
	for (const auto &[guid, metadata] : AssetManager::getAllMetadata())
	{
		if (!is_asset_of_type(metadata.sourcePath, asset_type))
			continue;

		eastl::string name = metadata.sourcePath.filename().string().c_str();
		name.make_lower();
		if (search.empty() || name.find(search) != eastl::string::npos)
			matched_paths.push_back(&metadata.sourcePath);
	}
	eastl::sort(matched_paths.begin(), matched_paths.end(), [](const std::filesystem::path *a, const std::filesystem::path *b)
	{
		return a->filename() < b->filename();
	});

	for (const std::filesystem::path *match : matched_paths)
	{
		std::string name = match->filename().string();
		if (ImGui::Selectable(name.c_str(), *match == *path))
			*path = *match;
		}
	}

static bool draw_asset_reference(AssetReference *reference, const AssetTypeInfo *accepted_type)
{
	std::filesystem::path current_path = AssetManager::getPath(*reference);
	std::filesystem::path picked_path = current_path;

	float browse_width = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
	ImGui::SetNextItemWidth(-browse_width);

	std::string asset_name = current_path.empty() ? "None" : current_path.filename().string();
	bool is_picker_open = ImGui::BeginCombo("##asset", asset_name.c_str(), ImGuiComboFlags_HeightLarge);
	if (is_picker_open)
	{
		draw_asset_search_picker(&picked_path, accepted_type);
		ImGui::EndCombo();
	}

	if (!is_picker_open && ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("DND_ASSET_PATH"))
		{
			std::filesystem::path dropped_path = (const char *)payload->Data;
			if (is_asset_of_type(dropped_path, accepted_type))
				picked_path = dropped_path;
		}
		ImGui::EndDragDropTarget();
	}

	if (!is_picker_open && ImGui::BeginPopupContextItem("##asset_menu"))
	{
		if (ImGui::MenuItem(ICON_FA_UP_RIGHT_FROM_SQUARE " Go to Asset", nullptr, false, !current_path.empty()))
		{
			EditorContext::current->selected_path = current_path;
			EditorContext::current->selection_type = EditorSelectionType::Asset;
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_ELLIPSIS))
	{
		std::filesystem::path chosen = Filesystem::openFileDialog().c_str();
		if (!chosen.empty() && is_asset_of_type(chosen, accepted_type))
			picked_path = chosen;
	}

	if (picked_path == current_path)
		return false;

	*reference = AssetReference(picked_path);
	return true;
}

bool UI::assetField(const char *label, AssetReference *reference, const AssetTypeInfo *accepted_type, const char *tooltip)
{
	bool can_expand = accepted_type && accepted_type->isAuthored() && reference->isValid();

	bool is_open = beginProperty(label, tooltip, can_expand);
	bool changed = draw_asset_reference(reference, accepted_type);
	if (is_open)
		changed |= drawAsset(*accepted_type, AssetManager::getPath(*reference));
	endProperty(is_open);
	return changed;
}

struct DrawerEntry
{
	const void *key = nullptr;
	UI::PropertyDrawer drawer = nullptr;
};

static eastl::vector<DrawerEntry> &getDrawers()
{
	static eastl::vector<DrawerEntry> drawers;
	return drawers;
}

bool UI::addDrawer(const void *key, PropertyDrawer drawer)
{
	getDrawers().push_back({key, drawer});
	return true;
}

static bool drawField(const FieldInfo &field, const char *label, void *value, void *owner);

static bool drawArray(const FieldInfo &field, const char *label, void *value)
{
	const ArrayInfo &array = *field.arrayInfo;
	int count = array.size(value);

	bool is_open = UI::beginProperty(label, field.tooltipText, count > 0);

	ImVec2 button_size(ImGui::GetFrameHeight(), ImGui::GetFrameHeight());

	bool changed = false;
	ImGui::Text("%d", count);
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_PLUS, button_size))
	{
		array.resize(value, count + 1);
		changed = true;
	}
	ImGui::SetItemTooltip("Add element");

	if (is_open)
	{
		FieldInfo element_field = array.element;
		element_field.registeredAssetType = field.registeredAssetType;

		int remove_index = -1;
		for (int i = 0; i < count; i++)
		{
			ImGui::PushID(i);
			eastl::string element_label = eastl::string("Element ") + eastl::to_string(i);

			if (ImGui::Button(ICON_FA_XMARK, button_size))
				remove_index = i;
			ImGui::SetItemTooltip("Remove element");
			ImGui::SameLine();

			void *element = array.at(value, i);
			changed |= drawField(element_field, element_label.c_str(), element, nullptr);
			ImGui::PopID();
		}

		if (remove_index >= 0)
		{
			array.remove(value, remove_index);
			changed = true;
		}
	}

	UI::endProperty(is_open);
	return changed;
}

static const char *calc_field_format(const FieldInfo &field, const char *fallback)
{
	return field.valueFormat ? field.valueFormat : fallback;
}

static bool drawField(const FieldInfo &field, const char *label, void *value, void *owner)
{
	// Find per field drawer
	for (const DrawerEntry &entry : getDrawers())
	{
		if (entry.key == &field)
			return entry.drawer(field, label, value, owner);
	}

	// Find per type drawer
	const void *type = field.valueInfo ? (const void *)field.valueInfo : (const void *)field.structInfo;
	for (const DrawerEntry &entry : getDrawers())
	{
		if (type && entry.key == type)
			return entry.drawer(field, label, value, owner);
	}

	const char *tooltip = field.tooltipText;

	if (field.isStruct<AssetReference>())
		return UI::assetField(label, (AssetReference *)value, field.getAssetType(), tooltip);

	if (field.structInfo)
	{
		ImGui::PushID(field.name);
		bool changed = false;
		if (UI::beginSection(label))
		{
			changed = UI::drawStruct(*field.structInfo, value);
			UI::endSection();
		}
		ImGui::PopID();
		return changed;
	}

	if (field.arrayInfo)
		return drawArray(field, label, value);

	if (field.enumItems)
	{
		if (field.isRadio)
			return UI::radio(label, (int *)value, field.enumItems, field.enumItemsCount, tooltip);
		return UI::combo(label, (int *)value, field.enumItems, field.enumItemsCount, tooltip);
	}

	if (field.isType<bool>())
		return UI::checkbox(label, (bool *)value, tooltip);

	if (field.isType<int>() || field.isType<uint32_t>())
	{
		if (field.hasRange())
			return UI::sliderInt(label, (int *)value, (int)field.minValue, (int)field.maxValue, calc_field_format(field, "%d"), field.isLogarithmic, tooltip);
		return UI::inputInt(label, (int *)value, tooltip);
	}

	if (field.isType<float>())
	{
		if (field.hasRange())
			return UI::sliderFloat(label, (float *)value, field.minValue, field.maxValue, calc_field_format(field, "%.3f"), field.isLogarithmic, tooltip);
		return UI::dragFloat(label, (float *)value, field.minValue, field.maxValue, calc_field_format(field, "%.3f"), field.isLogarithmic, tooltip);
	}

	if (field.isType<eastl::string>())
		return UI::inputText(label, (eastl::string *)value, tooltip);

	return false;
}

static const char *calc_category_title(const char *name)
{
	const char *title = name;
	for (const char *separator = strstr(title, " - "); separator; separator = strstr(title, " - "))
		title = separator + 3;
	return title;
}

static bool is_inside_category(const char *name, const char *outer)
{
	size_t length = strlen(outer);
	return strncmp(name, outer, length) == 0 && (name[length] == 0 || name[length] == ' ');
}

bool UI::drawStruct(const StructInfo &info, void *object)
{
	eastl::vector<const char *> open_categories;
	const char *hidden_by_category = nullptr;

	bool changed = false;
	for (int i = 0; i < info.fieldsCount; i++)
	{
		const FieldInfo &field = info.fields[i];
		if (field.isCategory())
		{
			if (hidden_by_category && is_inside_category(field.name, hidden_by_category))
				continue;
			hidden_by_category = nullptr;

			while (!open_categories.empty() && !is_inside_category(field.name, open_categories.back()))
			{
				endSection();
				open_categories.pop_back();
			}

			if (!open_categories.empty() && strcmp(open_categories.back(), field.name) == 0)
				continue;

			ImGui::PushID(field.name);
			bool is_open = beginSection(calc_category_title(field.name), true);
			ImGui::PopID();

			if (is_open)
				open_categories.push_back(field.name);
			else
				hidden_by_category = field.name;
			continue;
		}

		if (hidden_by_category || field.isHidden)
			continue;

		eastl::string label = field.displayLabel ? field.displayLabel : prettifyName(field.name);

		ImGui::BeginDisabled(!field.isEditable(object));
		changed |= drawField(field, label.c_str(), field.getAddress(object), object);
		ImGui::EndDisabled();
	}

	while (!open_categories.empty())
	{
		endSection();
		open_categories.pop_back();
	}
	return changed;
}

static eastl::vector<const Asset *> asset_drawn;
static const Asset *dirty_asset = nullptr;

bool UI::drawAsset(const AssetTypeInfo &type, const std::filesystem::path &path)
{
	Ref<Asset> asset = AssetManager::getAsset(path, &type);
	if (!asset)
		return false;

	if (eastl::find(asset_drawn.begin(), asset_drawn.end(), asset) != asset_drawn.end())
	{
		ImGui::TextDisabled("Recursive asset drawing");
		return false;
	}

	asset_drawn.push_back(asset);
	bool changed = drawStruct(*type.structInfo, asset);
	asset_drawn.pop_back();

	if (changed)
	{
		AssetManager::notifyChanged(asset);
		dirty_asset = asset;
	}

	// Save to file only when item is not active (not editing)
	if (dirty_asset == asset && !ImGui::IsAnyItemActive())
	{
		ReflectionYaml::saveToFile(*type.structInfo, asset, path);
		dirty_asset = nullptr;
	}
	return changed;
}

// Custom drawers
static bool draw_vec3(const FieldInfo &field, const char *label, void *value, void *owner)
{
	if (field.isColor)
		return UI::colorEdit3(label, (float *)value, field.tooltipText);
	return UI::dragFloat3(label, (float *)value, field.minValue, field.maxValue, calc_field_format(field, "%.3f"), field.isLogarithmic, field.tooltipText);
}
static const bool vec3_drawer_registered = UI::registerTypeDrawer<glm::vec3>(draw_vec3);

static bool draw_vec4(const FieldInfo &field, const char *label, void *value, void *owner)
{
	if (field.isColor)
		return UI::colorEdit4(label, (float *)value, field.tooltipText);
	return UI::dragFloat4(label, (float *)value, field.minValue, field.maxValue, calc_field_format(field, "%.3f"), field.isLogarithmic, field.tooltipText);
}
static const bool vec4_drawer_registered = UI::registerTypeDrawer<glm::vec4>(draw_vec4);
static const bool quat_drawer_registered = UI::registerTypeDrawer<glm::quat>(draw_vec4);

static bool draw_ivec3(const FieldInfo &field, const char *label, void *value, void *owner)
{
	return UI::dragInt3(label, (int *)value, (int)field.minValue, (int)field.maxValue, calc_field_format(field, "%d"), field.isLogarithmic, field.tooltipText);
}
static const bool ivec3_drawer_registered = UI::registerTypeDrawer<glm::ivec3>(draw_ivec3);
