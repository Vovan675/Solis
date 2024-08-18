#pragma once

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
	std::vector<FileEntry> get_directory_entries(const std::string &path);
	void render_directory(const std::string &path);

private:
	std::unordered_map<std::string, std::vector<FileEntry>> directories_cache;
	std::string root_path;
};

