#include "pch.h"
#include "AssetBrowserPanel.h"
#include "Scene/Scene.h"
#include "imgui/IconsFontAwesome6.h"
#include "imgui/ImGuiWrapper.h"
#include <imgui.h>
#include <filesystem>
#include "GuiUtils.h"

static std::string get_file_entension(const std::string &filename)
{
	return std::filesystem::path(filename).extension().string();
}

bool AssetBrowserPanel::renderImGui()
{
	auto &folder_tex = AssetManager::getTextureAsset("assets/editor/icons/folder.png");

	// Asset browser
	ImGui::Begin((std::string(ICON_FA_FOLDER) + " Asset Browser###Asset Browser").c_str());
	bool is_used = ImGui::IsWindowFocused();

	ImGui::BeginChild("hierarchy view", ImVec2(150, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
	if (ImGui::TreeNode("..."))
	{
		render_directory("assets");
		ImGui::TreePop();
	}
	ImGui::EndChild();
	ImGui::SameLine();

	float start_x = ImGui::GetCursorPosX();
	
	if (current_grid_path != root_path)
	{
		ImGui::PushFont(GuiUtils::roboto_regular_small);
		if (ImGui::Button(ICON_FA_ARROW_LEFT))
			current_grid_path = current_grid_path.parent_path();

		std::filesystem::path path_accumulated = root_path.parent_path();
		for (const auto& part : current_grid_path.relative_path())
		{
			ImGui::SameLine();

			ImGui::Text(">");
			ImGui::SameLine();

			path_accumulated /= part;

			bool already = path_accumulated == current_grid_path;

			if (already)
				ImGui::Text(part.generic_string().c_str());
			else if (ImGui::Button(part.generic_string().c_str()))
				current_grid_path = path_accumulated;
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
	auto entries = get_directory_entries(current_grid_path);
	for (const auto &entry : entries)
	{
		ImGui::PushID(entry.name.c_str());
		std::filesystem::path child_path = std::filesystem::path("assets") / entry.name;

		if (entry.isDirectory)
		{
			if (ImGui::ImageButton(ImGuiWrapper::getTextureDescriptorSet(folder_tex), tile_size, ImVec2(0, 1), ImVec2(1, 0)))
			{
				current_grid_path /= entry.name;
			}
		} else
		{
			auto file_icon = get_file_icon(child_path);
			ImGui::ImageButton(ImGuiWrapper::getTextureDescriptorSet(file_icon), tile_size, ImVec2(0, 1), ImVec2(1, 0));
			process_drag_drop_source(child_path);
			if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(0))
			{
				process_double_click(child_path);
			}
		}
		ImGui::Text(entry.name.c_str());
		
		ImGui::NextColumn();
		ImGui::PopID();
	}

	ImGui::Columns(1);

	ImGui::EndChild();

	ImGui::End();
	return is_used;
}

std::vector<FileEntry> AssetBrowserPanel::get_directory_entries(const std::filesystem::path &path)
{
	if (directories_cache.find(path) != directories_cache.end())
		return directories_cache[path];

	std::vector<FileEntry> entries;
	for (const auto &entry : std::filesystem::directory_iterator(path))
		entries.push_back({entry.path().filename().string(), entry.is_directory()});
	std::sort(entries.begin(), entries.end(), [](FileEntry a, FileEntry b) {
		if (a.isDirectory != b.isDirectory)
			return a.isDirectory ? true : false;
		return a.name < b.name;
	}
	);
	directories_cache[path] = entries;
	return entries;
}

void AssetBrowserPanel::render_directory(const std::string &path)
{
	auto entries = get_directory_entries(path);
	for (const auto &entry : entries)
	{
		if (entry.isDirectory)
		{
			if (ImGui::TreeNode(entry.name.c_str()))
			{
				std::filesystem::path child_path = (std::filesystem::path(path) / entry.name);
				render_directory(child_path.string());
				ImGui::TreePop();
			}
		} else
		{
			ImGui::TreeNodeEx(entry.name.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
			std::filesystem::path child_path = (std::filesystem::path(path) / entry.name);
			process_drag_drop_source(child_path);

			if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(0))
			{
				process_double_click(child_path);
			}
		}
	}
}

std::shared_ptr<Texture> AssetBrowserPanel::get_file_icon(std::filesystem::path &file)
{
	auto &file_tex = AssetManager::getTextureAsset("assets/editor/icons/file.png");
	auto &scene_tex = AssetManager::getTextureAsset("assets/editor/icons/scene.png");
	auto &texture_tex = AssetManager::getTextureAsset("assets/editor/icons/texture.png");

	std::string extension = file.extension().string();
	if (extension == ".scene")
		return scene_tex;
	else if (extension == ".png" || extension == ".jpg")
		return texture_tex;
	else
		return file_tex;
}

void AssetBrowserPanel::process_double_click(std::filesystem::path &file)
{
	std::string extension = file.extension().string();
	if (extension == ".scene")
	{
		Scene::loadScene(file.string());
	}
}

void AssetBrowserPanel::process_drag_drop_source(std::filesystem::path &file)
{
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
	{
		std::string path = file.string();
		ImGui::SetDragDropPayload("DND_ASSET_PATH", path.data(), sizeof(char) * (path.size() + 1));
		ImGui::Text("Copy");
		ImGui::EndDragDropSource();
	}
}
