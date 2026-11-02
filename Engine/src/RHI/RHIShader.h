#pragma once
#include "RHI/RHIDefinitions.h"

class RHIShader: public RefCounted
{
public:
	RHIShader(const eastl::wstring &path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines);
	virtual ~RHIShader();

	size_t getHash() const { return hash; }
	eastl::string getEntry() const { return entry_point.c_str(); }

	void recompile();

	static eastl::list<RHIShader *> getAllShadersAtPath(const eastl::wstring &path);

protected:
	virtual void load_bytecode(IDxcBlob *bytecode) = 0;

	size_t hash = 0;
	std::filesystem::path path;
	ShaderType type;
	eastl::string entry_point;
	eastl::vector<eastl::pair<eastl::string, eastl::string>> defines;
	eastl::hash_set<eastl::wstring> included_files;

	static eastl::unordered_map<eastl::wstring, eastl::list<RHIShader *>> path_to_shaders;
};
