#pragma once
#include <string>
#include <filesystem>

class Filesystem
{
public:
	static eastl::string saveFileDialog();
	static eastl::string openFileDialog();

	static eastl::wstring normalizePath(eastl::wstring path);
	static eastl::string normalizePath(eastl::string path);

	static eastl::string canonicalPath(const std::filesystem::path &path);
};