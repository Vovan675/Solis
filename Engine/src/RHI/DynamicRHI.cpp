#include "pch.h"
#include "DynamicRHI.h"
#include "Math/EngineMath.h"
#include "RHI/RHIPipeline.h"
#include "RHI/BindlessResources.h"
#include "Core/Filesystem.h"
#include "Core/Variables.h"

eastl::unordered_map<size_t, RHIShaderRef> DynamicRHI::cached_shaders;

eastl::wstring string_to_wstring(const eastl::string& s)
{
	DWORD size = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);

	eastl::wstring result;
	result.resize(size);

	MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, (LPWSTR)result.c_str(), size);

	return result;
}


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


RHIShaderRef DynamicRHI::createShader(eastl::wstring path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines)
{
	if (entry_point.empty())
	{
		if (type == VERTEX_SHADER)
			entry_point = "VSMain";
		else if (type == FRAGMENT_SHADER)
			entry_point = "PSMain";
		else if (type == COMPUTE_SHADER)
			entry_point = "CSMain";
		else if (type == RAY_GENERATION_SHADER)
			entry_point = "RayGen";
		else if (type == MISS_SHADER)
			entry_point = "Miss";
		else if (type == CLOSEST_HIT_SHADER)
			entry_point = "ClosestHit";
		else if (type == TASK_SHADER)
			entry_point = "TSMain";
		else if (type == MESH_SHADER)
			entry_point = "MSMain";
	}

	size_t cache_hash = 0;
	hashCombine(cache_hash, path);
	hashCombine(cache_hash, type);
	hashCombine(cache_hash, entry_point);

	eastl::string all_defines = "";
	for (const auto &define : defines)
	{
		all_defines += define.first;
		all_defines += define.second;
	}
	hashCombine(cache_hash, all_defines);

	auto it = cached_shaders.find(cache_hash);
	if (it != cached_shaders.end())
		return it->second;

	RHIShaderRef shader = create_shader(path, type, entry_point, defines);
	cached_shaders[cache_hash] = shader;
	return shader;
}

void DynamicRHI::setConstantBufferData(unsigned int binding, void *params_struct, size_t params_size)
{
	// If no descriptor set for this shader, create it
	size_t descriptor_hash = getCmdList()->current_pipeline->getHash();
	hashCombine(descriptor_hash, binding);
	hashCombine(descriptor_hash, frame_in_flight); // TODO: add Frame Allocator for these buffers

	current_bind_buffers[binding] = allocate_constant_buffer(descriptor_hash, params_struct, params_size);
	is_buffers_dirty = true;
}

void DynamicRHI::setConstantBufferDataPerFrame(unsigned int binding, void *params_struct, size_t params_size)
{
	size_t data_hash = 0;
	hashCombine(data_hash, binding);
	hashCombine(data_hash, params_size);
	hashCombine(data_hash, frame_in_flight);

	current_bind_buffers[binding] = allocate_constant_buffer(data_hash, params_struct, params_size);
	is_buffers_dirty = true;
}

RHIBuffer *DynamicRHI::allocate_constant_buffer(size_t descriptor_hash, void *params_struct, size_t params_size)
{
	// Create buffer if there is no for this descriptor and offset
	auto &buffers = buffers_for_shaders[descriptor_hash];

	if (buffers.buffers.size() <= buffers.current_offset)
	{
		BufferDescription desc;
		desc.size = params_size;
		desc.use_staging_buffer = false;
		desc.usage = BufferUsage::CONSTANT_BUFFER;
		ShaderDataBuffer data_buffer;
		data_buffer.buffer = createBuffer(desc);
		data_buffer.buffer->map(&data_buffer.mapped_data);

		buffers.buffers.push_back(data_buffer);
	}

	// Now we have buffer for this data in this descriptor
	auto &current_buffer = buffers.buffers[buffers.current_offset];
	memcpy(current_buffer.mapped_data, params_struct, params_size);
	buffers.current_offset++;
	return current_buffer.buffer;
}

DynamicRHI::CompileShaderResult DynamicRHI::compileShader(eastl::wstring path, ShaderType type, eastl::string entry_point, const eastl::vector<eastl::pair<eastl::string, eastl::string>> &defines)
{
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
	} else if (type == TASK_SHADER)
	{
		args.push_back(L"as_6_6");
	} else if (type == MESH_SHADER)
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
	if (isVulkan())
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

	CustomIncludeHandler include_handler(dxc_utils, dxc_include_handler);
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

	while (try_compile_shader() == false);

	CompileShaderResult result;
	dxc_results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&result.data), nullptr);
	result.source_hash = Engine::Math::fnv1aHash((const uint32_t *)result.data->GetBufferPointer(), result.data->GetBufferSize());
	result.included_files = include_handler.included_files;
	return result;
}

void DynamicRHI::advance_frame_in_flight(uint64_t frame_fence_value)
{
	frame_fence_values[frame_in_flight] = frame_fence_value;
	frame_in_flight = (frame_in_flight + 1) % MAX_FRAMES_IN_FLIGHT;

	PROFILE_CPU_SCOPE("Wait in flight fence");
	// Wait when queue for next frame is completed (device -> host sync)
	getCmdQueue()->waitForFence(frame_fence_values[frame_in_flight]);
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
