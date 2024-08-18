#include "pch.h"
#include "AssetBrowserPanel.h"
#include <imgui.h>
#include <filesystem>

static std::string get_file_entension(const std::string &filename)
{
	return std::filesystem::path(filename).extension().string();
}

bool AssetBrowserPanel::renderImGui()
{
	// Asset browser
	ImGui::Begin("Asset Browser");
	bool is_used = ImGui::IsWindowFocused();

	if (ImGui::TreeNode("..."))
	{
		render_directory("assets");
		ImGui::TreePop();
	}

	ImGui::End();
	return is_used;
}

std::vector<FileEntry> AssetBrowserPanel::get_directory_entries(const std::string &path)
{
	if (directories_cache.find(path) != directories_cache.end())
		return directories_cache[path];

	std::vector<FileEntry> entries;
	for (const auto &entry : std::filesystem::directory_iterator(path))
		entries.push_back({entry.path().filename().string(), entry.is_directory()});
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
				std::string child_path = (std::filesystem::path(path) / entry.name).string();
				render_directory(child_path);
				ImGui::TreePop();
			}
		} else
		{
			ImGui::TreeNodeEx(entry.name.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);

			if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(0))
			{
				if (get_file_entension(entry.name) == ".scene")
				{
					// TODO: load scene
				}
			}
		}
	}
}
