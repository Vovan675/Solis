#pragma once
#include <string>

class Filesystem
{
public:
	static eastl::string saveFileDialog();
	static eastl::string openFileDialog();

	static eastl::wstring normalizePath(eastl::wstring path);
	static eastl::string normalizePath(eastl::string path);
};