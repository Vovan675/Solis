#include "pch.h"
#include "Filesystem.h"
#include <filesystem>
#include <ShObjIdl_core.h>

std::string Filesystem::saveFileDialog()
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
		std::string path = ofn.lpstrFile;
		std::filesystem::current_path(currentPath);
		return std::filesystem::path(path).string();
	}
	return "";
}

std::string Filesystem::openFileDialog()
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
		std::string path = ofn.lpstrFile;
		std::filesystem::current_path(currentPath);
		return std::filesystem::path(path).string();
	}
	return "";
}
