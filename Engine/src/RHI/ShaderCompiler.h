#pragma once
#include "RHI/RHIDefinitions.h"

class ShaderCompiler
{
public:
	struct Result
	{
		ComPtr<IDxcBlob> data;
		size_t source_hash;
		eastl::hash_set<eastl::wstring> included_files;
	};

	ShaderCompiler();

	Result compile(eastl::wstring path, ShaderType type, eastl::string entry_point, const eastl::vector<eastl::pair<eastl::string, eastl::string>> &defines);
	void saveDirtyCache();

	IDxcUtils *getDxcUtils() const { return dxc_utils.Get(); }

private:
	struct CacheEntry
	{
		eastl::vector<eastl::wstring> includes;
		size_t sources_hash = 0;
		eastl::vector<uint8_t> bytecode;
	};

	ComPtr<IDxcUtils> dxc_utils;
	ComPtr<IDxcCompiler3> dxc_compiler;
	ComPtr<IDxcIncludeHandler> dxc_include_handler;

	uint64_t cache_version = 0;
	eastl::unordered_map<size_t, CacheEntry> cache;
	bool cache_dirty = false;
};
