#include "pch.h"
#include "ShaderCompiler.h"
#include "RHI/DynamicRHI.h"
#include "Math/EngineMath.h"
#include "RHI/BindlessResources.h"
#include "Core/Filesystem.h"
#include "Core/Variables.h"
#include "Utils/BinaryArchive.h"
#include "Utils/FileMemory.h"

static const char *CACHE_PATH = "cache/shader_cache.cache";
static const uint32_t CACHE_FORMAT_VERSION = 1;

class CustomIncludeHandler : public IDxcIncludeHandler
{
public:
	CustomIncludeHandler(IDxcUtils *dxc_utils, IDxcIncludeHandler *default_include_handler) : dxc_utils(dxc_utils), default_include_handler(default_include_handler) {};

	void clear()
	{
		included_files.clear();
		file_used_error = false;
	}

	HRESULT STDMETHODCALLTYPE LoadSource(_In_z_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource) override
	{
		ComPtr<IDxcBlobEncoding> encoding;
		eastl::wstring path = Filesystem::normalizePath(pFilename);
		if (included_files.contains(path))
		{
			// Return empty string blob if this file has been included before
			static const char null_str[] = " ";
			dxc_utils->CreateBlobFromPinned(null_str, ARRAYSIZE(null_str), DXC_CP_ACP, encoding.GetAddressOf());
			*ppIncludeSource = encoding.Detach();
			return S_OK;
		}

		HRESULT hr = dxc_utils->LoadFile(pFilename, nullptr, encoding.GetAddressOf());
		if (SUCCEEDED(hr))
		{
			included_files.insert(path);
			*ppIncludeSource = encoding.Detach();
		}
		file_used_error |= (hr == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION));
		return hr;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override
	{
		return default_include_handler->QueryInterface(riid, ppvObject);
	}

	ULONG STDMETHODCALLTYPE AddRef(void) override { return 1; }
	ULONG STDMETHODCALLTYPE Release(void) override { return 1; }

	eastl::hash_set<eastl::wstring> included_files;
	bool file_used_error = false;

private:
	IDxcUtils *dxc_utils;
	IDxcIncludeHandler *default_include_handler;
};

ShaderCompiler::ShaderCompiler()
{
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxc_utils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc_compiler));
	dxc_utils->CreateDefaultIncludeHandler(&dxc_include_handler);

	// Load cache
	ComPtr<IDxcVersionInfo> version_info;
	dxc_compiler->QueryInterface(IID_PPV_ARGS(&version_info));
	uint32_t dxc_version_major, dxc_version_minor;
	version_info->GetVersion(&dxc_version_major, &dxc_version_minor);

	cache_version = dxc_version_major;
	Engine::Math::hashCombine(cache_version, dxc_version_minor);
	Engine::Math::hashCombine(cache_version, CACHE_FORMAT_VERSION);

	FileMemory file;
	if (!file.open(CACHE_PATH, true, 0))
		return;

	BinaryArchive archive = BinaryArchive::createForLoading((const uint8_t *)file.getData(), file.getSize(), 0);
	uint64_t version = 0;
	uint32_t count = 0;
	archive << version;
	if (version != cache_version)
		return;

	archive << count;
	for (int i = 0; i < count; i++)
	{
		size_t key = 0;
		archive << key;
		CacheEntry &entry = cache[key];
		archive << entry.includes << entry.sources_hash << entry.bytecode;
	}
}

ShaderCompiler::Result ShaderCompiler::compile(eastl::wstring path, ShaderType type, eastl::string entry_point, const eastl::vector<eastl::pair<eastl::string, eastl::string>> &defines)
{
	PROFILE_CPU_FUNCTION();
	eastl::wstring entry_point_wstr = unicode_to_wstring(entry_point.c_str());
	eastl::vector<LPCWSTR> args =
	{
		path.c_str(),            // Optional shader_blob source file name for error reporting
		// and for PIX shader_blob source view.
		L"-E", entry_point_wstr.c_str(),              // Entry point.
	};

	if (engine_shader_debug_info)
	{
		args.push_back(L"-Zi");
		args.push_back(L"-Qembed_debug");
	}

	args.push_back(L"-T");
	if (type == ShaderType::VERTEX)
	{
		args.push_back(L"vs_6_6");
	} else if (type == ShaderType::FRAGMENT)
	{
		args.push_back(L"ps_6_6");
	} else if (type == ShaderType::COMPUTE)
	{
		args.push_back(L"cs_6_6");
	} else if (type == ShaderType::RAY_GENERATION)
	{
		args.push_back(L"lib_6_6");
	} else if (type == ShaderType::MISS)
	{
		args.push_back(L"lib_6_6");
	} else if (type == ShaderType::CLOSEST_HIT)
	{
		args.push_back(L"lib_6_6");
	} else if (type == ShaderType::TASK)
	{
		args.push_back(L"as_6_6");
	} else if (type == ShaderType::MESH)
	{
		args.push_back(L"ms_6_6");
	}

	args.push_back(DXC_ARG_PACK_MATRIX_COLUMN_MAJOR);

	// Third party libraries
	args.push_back(L"-I");
	args.push_back(L"../vendor/rtxdi/Include");
	args.push_back(L"-I");
	args.push_back(L"../vendor/nrd/Shaders");
	args.push_back(L"-I");
	args.push_back(L"../vendor/MathLib");

	eastl::wstring bindless_resource_heap_binding = eastl::to_wstring(BINDLESS_RESOURCES_BINDING);
	eastl::wstring bindless_resource_heap_set = eastl::to_wstring(BINDLESS_RESOURCES_SET);

	eastl::wstring bindless_samplers_heap_binding = eastl::to_wstring(BINDLESS_SAMPLERS_BINDING);
	eastl::wstring bindless_samplers_heap_set = eastl::to_wstring(BINDLESS_SAMPLERS_SET);
	if (gDynamicRHI->isVulkan())
	{
		args.push_back(L"-spirv");
		args.push_back(L"-D");
		args.push_back(L"VULKAN");

		args.push_back(L"-fvk-bind-resource-heap");
		args.push_back(bindless_resource_heap_binding.c_str());
		args.push_back(bindless_resource_heap_set.c_str());

		args.push_back(L"-fvk-bind-sampler-heap");
		args.push_back(bindless_samplers_heap_binding.c_str());
		args.push_back(bindless_samplers_heap_set.c_str());

		args.push_back(L"-fvk-use-dx-layout");
		args.push_back(L"-fvk-auto-shift-bindings");
		args.push_back(L"-fspv-target-env=vulkan1.3");
	} else
	{
		args.push_back(L"-Wno-ignored-attributes");
	}

	if (type == ShaderType::RAY_GENERATION || type == ShaderType::MISS || type == ShaderType::CLOSEST_HIT)
	{
		args.push_back(L"-D");
		args.push_back(L"RAY_TRACING_SHADER");
	}

	eastl::vector<eastl::wstring> wdefines;
	for (const auto &define : defines)
	{
		eastl::wstring wstr;
		wstr.assign_convert(define.first + "=" + define.second);
		wdefines.emplace_back(wstr);
	}

	for (const auto &def : wdefines)
	{
		args.push_back(L"-D");
		args.push_back(def.c_str());
	}

	ComPtr<IDxcResult> dxc_results;

	CustomIncludeHandler include_handler(dxc_utils.Get(), dxc_include_handler.Get());
	auto try_compile_shader = [&]()
	{
		include_handler.clear();

		ComPtr<IDxcBlobEncoding> pSource = nullptr;
		HRESULT res = dxc_utils->LoadFile(path.c_str(), nullptr, &pSource);
		if (FAILED(res))
			return false;
		DxcBuffer Source;
		Source.Ptr = pSource->GetBufferPointer();
		Source.Size = pSource->GetBufferSize();
		Source.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.


		dxc_compiler->Compile(
			&Source,
			args.data(),
			args.size(),
			&include_handler,
			IID_PPV_ARGS(&dxc_results)
		);

		if (include_handler.file_used_error)
			return false;

		ComPtr<IDxcBlobUtf8> pErrors = nullptr;
		dxc_results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
		if (pErrors != nullptr && pErrors->GetStringLength() != 0)
		{
			CORE_WARN("Warnings and Errors:\n{}\n", pErrors->GetStringPointer());
			eastl::string message = "Fix shader error: \n";
			message += pErrors->GetStringPointer();
			MessageBoxA(NULL, message.c_str(), NULL, MB_OK);
			return false;
		}

		// Quit if the compilation failed.
		HRESULT result_status;
		dxc_results->GetStatus(&result_status);
		if (FAILED(result_status))
		{
			CORE_ERROR("Compilation Failed\n");
			eastl::string message = "Fix shader errors";
			MessageBoxA(NULL, message.c_str(), NULL, MB_OK);
			return false;
		}
		return true;
	};

	auto hash_file = [&](const eastl::wstring &file)
	{
		ComPtr<IDxcBlobEncoding> source;
		dxc_utils->LoadFile(file.c_str(), nullptr, source.GetAddressOf());
		return source ? Engine::Math::fnv1aHash(source->GetBufferPointer(), source->GetBufferSize()) : 0;
	};

	auto hash_sources = [&](const eastl::vector<eastl::wstring> &includes)
	{
		size_t hash = hash_file(path);
		for (const eastl::wstring &include : includes)
			Engine::Math::hashCombine(hash, hash_file(include));
		return hash;
	};

	// Cache key = hash of args
	size_t cache_key = 0;
	for (LPCWSTR arg : args)
		Engine::Math::hashCombine(cache_key, arg);

	CacheEntry &entry = cache[cache_key];
	size_t sources_hash = hash_sources(entry.includes);

	// Recalculate cache only if its empty or any used file changed
	if (entry.bytecode.empty() || sources_hash != entry.sources_hash)
	{
		while (try_compile_shader() == false);

		ComPtr<IDxcBlob> bytecode;
		dxc_results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&bytecode), nullptr);
		const uint8_t *bytecode_data = (const uint8_t *)bytecode->GetBufferPointer();
		entry.bytecode.assign(bytecode_data, bytecode_data + bytecode->GetBufferSize());
		entry.includes.assign(include_handler.included_files.begin(), include_handler.included_files.end());
		entry.sources_hash = hash_sources(entry.includes);
		cache_dirty = true;
	}

	Result result;
	ComPtr<IDxcBlobEncoding> blob;
	dxc_utils->CreateBlob(entry.bytecode.data(), entry.bytecode.size(), DXC_CP_ACP, blob.GetAddressOf());
	result.data = blob;
	result.source_hash = Engine::Math::fnv1aHash(result.data->GetBufferPointer(), result.data->GetBufferSize());
	result.included_files.insert(entry.includes.begin(), entry.includes.end());
	return result;
}

void ShaderCompiler::saveDirtyCache()
{
	if (!cache_dirty)
		return;
	cache_dirty = false;

	std::filesystem::create_directories("cache");
	std::ofstream file(CACHE_PATH, std::ios::binary);
	BinaryArchive archive = BinaryArchive::createForSaving(file);
	archive << cache_version;
	uint32_t count = cache.size();
	archive << count;
	for (auto &it : cache)
	{
		size_t key = it.first;
		archive << key << it.second.includes << it.second.sources_hash << it.second.bytecode;
	}
}
