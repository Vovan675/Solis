#pragma once
#include "RHI/RHITexture.h"
#include "EditorContext.h"

struct FileEntry
{
	eastl::string name;
	bool isDirectory;
};

class AssetBrowserPanel
{
public:
	void init();

	void setRootPath(eastl::string path) { root_path = path.c_str(); }
	bool renderImGui(EditorContext &context);

	void setCurrentAsset(const std::filesystem::path &path) { current_path = path.parent_path(); }

private:
	std::vector<FileEntry> get_directory_entries(const std::filesystem::path &path);
	bool has_subdirectories(const std::filesystem::path &path);
	void draw_directories_tree(const eastl::string &path);

	RHITextureRef get_file_icon(std::filesystem::path &file);
	void process_double_click(std::filesystem::path &file);
	void process_drag_drop_source(std::filesystem::path &file);
	void process_asset_context_menu(const std::filesystem::path &file, EditorContext &context);
	void draw_rename_popup(EditorContext &context);
private:
	std::unordered_map<std::filesystem::path, std::vector<FileEntry>> directories_cache;
	std::filesystem::path root_path = "assets";
	std::filesystem::path current_path = "assets";

	std::filesystem::path rename_target;
	char rename_buffer[256] = {};
	bool open_rename_popup = false;

	RHITextureRef file_texture;
	RHITextureRef scene_texture;
	RHITextureRef texture_texture;
};

