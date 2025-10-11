#include "pch.h"
#include "RHIShader.h"
#include "Core/Filesystem.h"

eastl::unordered_map<eastl::wstring, eastl::list<RHIShader *>> RHIShader::path_to_shaders;

RHIShader::RHIShader(const eastl::wstring &path, ShaderType type, eastl::wstring entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines)
    : path(path.c_str()), type(type), entry_point(entry_point), defines(defines)
{
    path_to_shaders[Filesystem::normalizePath(path.c_str())].push_back(this);
}

RHIShader::~RHIShader()
{
    path_to_shaders[Filesystem::normalizePath(path.c_str())].remove(this);
}

eastl::list<RHIShader *> RHIShader::getAllShadersAtPath(const eastl::wstring &path)
{
    return path_to_shaders[Filesystem::normalizePath(path)];
}
