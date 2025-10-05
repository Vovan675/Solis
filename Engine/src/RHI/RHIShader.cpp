#include "pch.h"
#include "RHIShader.h"

std::unordered_map<std::filesystem::path, std::list<RHIShader *>> RHIShader::path_to_shaders;

RHIShader::RHIShader(const std::wstring &path, ShaderType type, std::wstring entry_point, std::vector<std::pair<const char *, const char *>> defines)
    : path(path), type(type), entry_point(entry_point), defines(defines)
{
    path_to_shaders[path].push_back(this);
}

RHIShader::~RHIShader()
{
    path_to_shaders[path].remove(this);
}

std::list<RHIShader *> RHIShader::getAllShadersAtPath(const std::wstring &path)
{
    return path_to_shaders[path];
}
