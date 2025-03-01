#pragma once
#include "RHI/RHITexture.h"

struct FileEntry
{
	std::string name;
	bool isDirectory;
};

class AssetBrowserPanel
{
public:
	void setRootPath(std::string path) { root_path = path; }
	bool renderImGui();

private:
	std::vector<FileEntry> get_directory_entries(const std::filesystem::path &path);
	void render_directory(const std::string &path);

	std::shared_ptr<RHITexture> get_file_icon(std::filesystem::path &file);
	void process_double_click(std::filesystem::path &file);
	void process_drag_drop_source(std::filesystem::path &file);
private:
	std::unordered_map<std::filesystem::path, std::vector<FileEntry>> directories_cache;
	std::filesystem::path root_path = "assets";
	std::filesystem::path current_grid_path = "assets";
};

