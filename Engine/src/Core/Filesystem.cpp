#include "pch.h"
#include "Filesystem.h"
#include <filesystem>
#include <ShObjIdl_core.h>
#include <commdlg.h>

eastl::string Filesystem::saveFileDialog()
{
	std::filesystem::path currentPath = std::filesystem::current_path();

	char szFile[_MAX_PATH] = "name";

	OPENFILENAMEA ofn{sizeof(OPENFILENAMEA)};
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	//ofn.lpstrFilter = "";
	ofn.Flags = OFN_PATHMUSTEXIST;

	if (GetSaveFileNameA(&ofn))
	{
		eastl::string path = ofn.lpstrFile;
		std::filesystem::current_path(currentPath);
		return Filesystem::normalizePath(path);
	}
	return "";
}

eastl::string Filesystem::openFileDialog()
{
	std::filesystem::path currentPath = std::filesystem::current_path();

	char szFile[_MAX_PATH] = "name";

	OPENFILENAMEA ofn { sizeof(OPENFILENAMEA) };
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	//ofn.lpstrFilter = "";
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileNameA(&ofn))
	{
		eastl::string path = ofn.lpstrFile;
		std::filesystem::current_path(currentPath);
		return Filesystem::normalizePath(path);
	}
	return "";
}

eastl::wstring Filesystem::normalizePath(eastl::wstring path)
{
	std::filesystem::path p = path.c_str();
	return p.lexically_normal().wstring().c_str();
}

eastl::string Filesystem::normalizePath(eastl::string path)
{
	std::filesystem::path p = path.c_str();
	return p.lexically_normal().string().c_str();
}

eastl::string Filesystem::canonicalPath(const std::filesystem::path &path)
{
	std::error_code ec;
	std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
	if (ec || canonical.empty())
		canonical = path.lexically_normal();
	return canonical.generic_string().c_str();
}
