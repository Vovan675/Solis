#include "pch.h"
#include "DynamicRHI.h"
#include "Math/EngineMath.h"
#include "RHI/RHIShader.h"
#include "RHI/BindlessResources.h"

std::unordered_map<size_t, RHIShaderRef> DynamicRHI::cached_shaders;

std::wstring string_to_wstring(const std::string& s)
{
	DWORD size = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);

	std::wstring result;
	result.resize(size);

	MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, (LPWSTR)result.c_str(), size);

	return result;
}

ComPtr<IDxcBlob> DynamicRHI::compile_shader(std::wstring path, ShaderType type, std::wstring entry_point, bool is_vulkan, size_t &source_hash, std::vector<std::pair<const char *, const char *>> *defines)
{
	std::vector<LPCWSTR> args =
	{
		path.c_str(),            // Optional shader_blob source file name for error reporting
		// and for PIX shader_blob source view.  
		L"-E", entry_point.c_str(),              // Entry point.
		L"-Zs",                      // Enable debug information (slim format)
		//L"-Zi",
		//L"-Qsource_in_debug_module", 
		//L"-Qstrip_reflect",          // Strip reflection into a separate blob. 
	};

	args.push_back(L"-T");
	if (type == VERTEX_SHADER)
	{
		args.push_back(L"vs_6_6");
	} else if (type == FRAGMENT_SHADER)
	{
		args.push_back(L"ps_6_6");
	} else if (type == COMPUTE_SHADER)
	{
		args.push_back(L"cs_6_6");
	} else if (type == RAY_GENERATION_SHADER)
	{
		args.push_back(L"lib_6_6");
	} else if (type == MISS_SHADER)
	{
		args.push_back(L"lib_6_6");
	} else if (type == CLOSEST_HIT_SHADER)
	{
		args.push_back(L"lib_6_6");
	}

	args.push_back(DXC_ARG_PACK_MATRIX_COLUMN_MAJOR);

	std::wstring bindless_resource_heap_binding = std::to_wstring(BINDLESS_TEXTURES_BINDING);
	std::wstring bindless_resource_heap_set = std::to_wstring(BINDLESS_TEXTURES_SET);

	std::wstring bindless_samplers_heap_binding = std::to_wstring(BINDLESS_SAMPLERS_BINDING);
	std::wstring bindless_samplers_heap_set = std::to_wstring(BINDLESS_SAMPLERS_SET);
	if (is_vulkan)
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

	if (type == RAY_GENERATION_SHADER || type == MISS_SHADER || type == CLOSEST_HIT_SHADER)
	{
		args.push_back(L"-D");
		args.push_back(L"RAY_TRACING_SHADER");
	}

	std::vector<std::string> defines_args;

	std::vector<std::wstring> wdefines;
	if (defines)
	{
		for (const auto &define : *defines)
		{
			std::string key = std::string(define.first);
			std::string value = std::string(define.second);

			std::string full_define = key + "=" + value;

			wdefines.emplace_back(full_define.begin(), full_define.end());
		}

		for (const auto &def : wdefines)
		{
			args.push_back(L"-D");
			args.push_back(def.c_str());
		}
	}

	ComPtr<IDxcResult> dxc_results;

	auto try_compile_shader = [&]()
	{
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
			dxc_include_handler,
			IID_PPV_ARGS(&dxc_results)
		);

		ComPtr<IDxcBlobUtf8> pErrors = nullptr;
		dxc_results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
		if (pErrors != nullptr && pErrors->GetStringLength() != 0)
		{
			CORE_WARN("Warnings and Errors:\n{}\n", pErrors->GetStringPointer());
			std::string message = "Fix shader error: \n";
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
			std::string message = "Fix shader errors";
			MessageBoxA(NULL, message.c_str(), NULL, MB_OK);
			return false;
		}
		return true;
	};

	while (try_compile_shader() == false);

	ComPtr<IDxcBlob> shader_blob;
	dxc_results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_blob), nullptr);

	source_hash = Engine::Math::fnv1aHash((const uint32_t *)shader_blob->GetBufferPointer(), shader_blob->GetBufferSize());
	return shader_blob;
}

void DynamicRHI::release_gpu_resources(uint64_t frame)
{
	while (!gpu_release_queue.empty())
	{
		if (gpu_release_queue.front().release_frame >= frame)
			break;

		gpu_release_queue.front().resource->Release();
		delete gpu_release_queue.front().resource;
		gpu_release_queue.pop();
	}
}
