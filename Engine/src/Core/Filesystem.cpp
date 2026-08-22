#include "pch.h"
#include "Filesystem.h"
#include <filesystem>
#include <ShObjIdl_core.h>
#include <commdlg.h>

static eastl::string file_dialog(bool is_save, const char *name)
{
	char file_name[MAX_PATH];
	snprintf(file_name, sizeof(file_name), "%s", name);

	OPENFILENAMEA ofn{sizeof(OPENFILENAMEA)};
	ofn.hwndOwner = GetActiveWindow();
	ofn.lpstrFile = file_name;
	ofn.nMaxFile = sizeof(file_name);
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	if (!is_save)
		ofn.Flags |= OFN_FILEMUSTEXIST;

	bool picked = is_save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
	if (!picked)
		return "";
	return Filesystem::normalizePath(eastl::string(ofn.lpstrFile));
}

eastl::string Filesystem::saveFileDialog()
{
	return file_dialog(true, "name");
}

eastl::string Filesystem::openFileDialog()
{
	return file_dialog(false, "");
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
