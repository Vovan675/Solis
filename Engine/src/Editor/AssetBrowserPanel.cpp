#include "pch.h"
#include "AssetBrowserPanel.h"
#include "Scene/Scene.h"
#include "imgui/IconsFontAwesome6.h"
#include "imgui/ImGuiWrapper.h"
#include <imgui.h>
#include <filesystem>
#include "UI.h"

static eastl::string get_file_entension(const eastl::string &filename)
{
	return std::filesystem::path(filename.c_str()).extension().string().c_str();
}

void AssetBrowserPanel::init()
{
	root_path = AssetManager::getAssetsRoot();
	current_path = root_path;

	file_texture = AssetManager::getTextureAsset("assets/editor/icons/file.png");
	scene_texture = AssetManager::getTextureAsset("assets/editor/icons/scene.png");
	texture_texture = AssetManager::getTextureAsset("assets/editor/icons/texture.png");
}

bool AssetBrowserPanel::renderImGui(EditorContext &context)
{
	if (context.selected_path != last_selected_path)
	{
		last_selected_path = context.selected_path;
		if (context.selection_type == EditorSelectionType::Asset && !context.selected_path.empty())
			current_path = context.selected_path.parent_path();
	}

	auto &folder_tex = AssetManager::getTextureAsset("assets/editor/icons/folder.png");

	// Asset browser
	ImGui::Begin((eastl::string(ICON_FA_FOLDER) + " Asset Browser###Asset Browser").c_str());
	bool is_used = ImGui::IsWindowFocused();

	ImGui::BeginChild("hierarchy view", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
	if (ImGui::TreeNode("..."))
	{
		draw_directories_tree(root_path.string().c_str());
		ImGui::TreePop();
	}
	ImGui::EndChild();
	ImGui::SameLine();

	float start_x = ImGui::GetCursorPosX();
	
	if (current_path != root_path)
	{
		ImGui::PushFont(UI::font_small);
		if (ImGui::Button(ICON_FA_ARROW_LEFT))
			current_path = current_path.parent_path();

		std::filesystem::path path_accumulated = root_path.parent_path();
		for (const auto& part : current_path.relative_path())
		{
			ImGui::SameLine();

			ImGui::Text(">");
			ImGui::SameLine();

			path_accumulated /= part;

			bool already = path_accumulated == current_path;

			if (already)
				ImGui::Text(part.generic_string().c_str());
			else if (ImGui::Button(part.generic_string().c_str()))
				current_path = path_accumulated;
		}
		ImGui::PopFont();
	}

	ImGui::SetCursorPos({start_x, 60 });
	ImGui::BeginChild("grid view");
	ImGui::SetCursorPosY(ImGui::GetStyle().ItemSpacing.x);

	ImVec2 available_region = ImGui::GetContentRegionAvail();
	ImVec2 padding = ImVec2(16, 16);
	ImVec2 tile_size = ImVec2(128, 128);
	int columns = available_region.x / (tile_size.x + padding.x);

	ImGui::Columns(std::max(1, columns), 0, false);

	int column = 0;
	auto entries = get_directory_entries(current_path);
	for (const auto &entry : entries)
	{
		ImGui::PushID(entry.name.c_str());
		std::filesystem::path child_path = current_path / entry.name.c_str();

		if (entry.isDirectory)
		{
			if (ImGui::ImageButton(entry.name.c_str(), ImGuiWrapper::getTextureId(folder_tex), tile_size, {0, 0}, {1, 1}))
			{
				current_path /= entry.name.c_str();
			}
		} else
		{
			auto file_icon = get_file_icon(child_path);
			ImVec4 color = child_path == context.selected_path ? ImVec4(0.6, 0.6, 0.6, 1.0) : ImVec4();
			ImGui::ImageButton(entry.name.c_str(), ImGuiWrapper::getTextureId(file_icon), tile_size, {0, 0}, {1, 1}, color);
			process_drag_drop_source(child_path);
			process_asset_context_menu(child_path, context);
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				process_double_click(child_path);
			} else if (ImGui::IsItemDeactivated())
			{
				ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
				if (drag.x == 0.0f && drag.y == 0.0f)
				{
					context.selected_path = child_path;
					context.selection_type = EditorSelectionType::Asset;
				}
			}
		}
		ImGui::Text(entry.name.c_str());

		ImGui::NextColumn();
		ImGui::PopID();
	}

	ImGui::Columns(1);

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2) && !context.selected_path.empty())
		open_rename(context.selected_path);

	process_create_menu(context);
	draw_rename_popup(context);

	ImGui::EndChild();

	ImGui::End();
	return is_used;
}

void AssetBrowserPanel::process_create_menu(EditorContext &context)
{
	if (!ImGui::BeginPopupContextWindow("CreateAsset", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		return;

	if (ImGui::BeginMenu("Create"))
	{
		for (const AssetTypeInfo *type : AssetManager::getTypeInfos())
		{
			if (!type->isAuthored())
				continue;

			if (!ImGui::MenuItem(type->name))
				continue;

			std::filesystem::path path;
			// Find not occupied by other file path
			for (int index = 0; path.empty() || std::filesystem::exists(path); index++)
				path = current_path / fmt::format("New{}{}{}", type->name, index > 0 ? std::to_string(index) : "", type->extensions.front().c_str());

			ReflectionYaml::saveToFile(*type->structInfo, type->structInfo->defaults, path);
			AssetManager::getOrCreateMetadata(path);

			refreshCache();
			context.selected_path = path;
			context.selection_type = EditorSelectionType::Asset;
		}
		ImGui::EndMenu();
	}
	ImGui::EndPopup();
}

void AssetBrowserPanel::process_asset_context_menu(const std::filesystem::path &file, EditorContext &context)
{
	if (!ImGui::BeginPopupContextItem())
		return;

	if (ImGui::MenuItem("Rename", "F2"))
		open_rename(file);
	if (ImGui::MenuItem("Delete"))
	{
		AssetManager::deleteAsset(file);
		if (context.selected_path == file)
			context.selected_path.clear();
		refreshCache();
	}
	ImGui::EndPopup();
}

void AssetBrowserPanel::open_rename(const std::filesystem::path &file)
{
	rename_target = file;
	eastl::string name = file.filename().string().c_str();
	strncpy(rename_buffer, name.c_str(), sizeof(rename_buffer) - 1);
	rename_buffer[sizeof(rename_buffer) - 1] = 0;
	open_rename_popup = true;
}

void AssetBrowserPanel::draw_rename_popup(EditorContext &context)
{
	if (open_rename_popup)
	{
		ImGui::OpenPopup("Rename Asset");
		open_rename_popup = false;
	}

	if (!ImGui::BeginPopupModal("Rename Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	bool confirmed = ImGui::InputText("Name", rename_buffer, sizeof(rename_buffer), ImGuiInputTextFlags_EnterReturnsTrue);

	if ((ImGui::Button("OK") || confirmed) && rename_buffer[0])
	{
		std::filesystem::path new_path = rename_target.parent_path() / rename_buffer;
		AssetManager::moveAsset(rename_target, new_path);
		if (context.selected_path == rename_target)
			context.selected_path = new_path;
		refreshCache();
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
		ImGui::CloseCurrentPopup();

	ImGui::EndPopup();
}

std::vector<FileEntry> AssetBrowserPanel::get_directory_entries(const std::filesystem::path &path)
{
	if (directories_cache.find(path) != directories_cache.end())
		return directories_cache[path];

	std::vector<FileEntry> entries;
	for (const auto &entry : std::filesystem::directory_iterator(path))
	{
		if (entry.path().extension() == ".meta")
			continue;

		FileEntry file_entry;
		file_entry.name = entry.path().filename().string().c_str();
		file_entry.isDirectory = entry.is_directory();
		entries.push_back(file_entry);
	}
	eastl::sort(entries.begin(), entries.end(), [](FileEntry a, FileEntry b) {
		if (a.isDirectory != b.isDirectory)
			return a.isDirectory ? true : false;
		return a.name < b.name;
	}
	);
	directories_cache[path] = entries;
	return entries;
}

bool AssetBrowserPanel::has_subdirectories(const std::filesystem::path &path)
{
	auto entries = get_directory_entries(path);
	for (const auto &entry : entries)
	{
		if (entry.isDirectory)
			return true;
	}
	return false;
}

void AssetBrowserPanel::draw_directories_tree(const eastl::string &path)
{
	auto entries = get_directory_entries(path.c_str());
	for (const auto &entry : entries)
	{
		if (!entry.isDirectory)
			continue;

		std::filesystem::path child_path = (std::filesystem::path(path.c_str()) / entry.name.c_str());

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (current_path == child_path)
			flags |= ImGuiTreeNodeFlags_Selected;
		if (!has_subdirectories(child_path))
			flags |= ImGuiTreeNodeFlags_Leaf;

		bool open = ImGui::TreeNodeEx(entry.name.c_str(), flags);

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			current_path = child_path;

		if (open)
		{
			draw_directories_tree(child_path.string().c_str());
			ImGui::TreePop();
		}
	}
}

RHITextureRef AssetBrowserPanel::get_file_icon(std::filesystem::path &file)
{
	eastl::string extension = file.extension().string().c_str();
	if (extension == ".scene")
		return scene_texture;
	else if (extension == ".png" || extension == ".jpg")
		return texture_texture;
	else
		return file_texture;
}

void AssetBrowserPanel::process_double_click(std::filesystem::path &file)
{
	eastl::string extension = file.extension().string().c_str();
	if (extension == ".scene")
	{
		Scene::loadScene(file.string().c_str());
	}
}

void AssetBrowserPanel::process_drag_drop_source(std::filesystem::path &file)
{
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		eastl::string path = file.string().c_str();
		ImGui::SetDragDropPayload("DND_ASSET_PATH", path.data(), sizeof(char) * (path.size() + 1));
		ImGui::Text("Copy");
		ImGui::EndDragDropSource();
	}
}
