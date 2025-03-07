#include "pch.h"
#include "DynamicRHI.h"
#include "EngineMath.h"
#include "RHI/RHIShader.h"

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
		path.c_str(),            // Optional shader source file name for error reporting
		// and for PIX shader source view.  
		L"-E", entry_point.c_str(),              // Entry point.
		L"-Zs",                      // Enable debug information (slim format)
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
	}

	args.push_back(DXC_ARG_PACK_MATRIX_COLUMN_MAJOR);

	if (is_vulkan)
	{
		args.push_back(L"-spirv");
		args.push_back(L"-D");
		args.push_back(L"VULKAN");
		args.push_back(L"-fvk-use-dx-layout");
		args.push_back(L"-fvk-auto-shift-bindings");
	}

	std::vector<std::wstring> wargs;
	if (defines)
	{
		for (auto &define : *defines)
		{
			args.push_back(L"-D");
			std::string arg = define.first;
			arg += "=";
			arg += define.second;
			auto &warg = wargs.emplace_back(std::wstring(arg.begin(), arg.end()));
			args.push_back(warg.c_str());
		}
	}

	//
	// Open source file.  
	//
	ComPtr<IDxcBlobEncoding> pSource = nullptr;
	dxc_utils->LoadFile(path.c_str(), nullptr, &pSource);
	DxcBuffer Source;
	Source.Ptr = pSource->GetBufferPointer();
	Source.Size = pSource->GetBufferSize();
	Source.Encoding = DXC_CP_ACP; // Assume BOM says UTF8 or UTF16 or this is ANSI text.


	//
	// Compile it with specified arguments.
	//
	ComPtr<IDxcResult> pResults;
	dxc_compiler->Compile(
		&Source,                // Source buffer.
		args.data(),                // Array of pointers to arguments.
		args.size(),      // Number of arguments.
		dxc_include_handler,
		IID_PPV_ARGS(&pResults) // Compiler output status, buffer, and errors.
	);

	//
	// Print errors if present.
	//
	ComPtr<IDxcBlobUtf8> pErrors = nullptr;
	pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
	// Note that d3dcompiler would return null if no errors or warnings are present.
	// IDxcCompiler3::Compile will always return an error buffer, but its length
	// will be zero if there are no warnings or errors.
	if (pErrors != nullptr && pErrors->GetStringLength() != 0)
		wprintf(L"Warnings and Errors:\n%S\n", pErrors->GetStringPointer());

	//
	// Quit if the compilation failed.
	//
	HRESULT hrStatus;
	pResults->GetStatus(&hrStatus);
	if (FAILED(hrStatus))
	{
		wprintf(L"Compilation Failed\n");
	}

	ComPtr<IDxcBlob> shader;
	pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader), nullptr);

	source_hash = Engine::Math::fnv1aHash((const uint32_t *)shader->GetBufferPointer(), shader->GetBufferSize());
	return shader;
}
