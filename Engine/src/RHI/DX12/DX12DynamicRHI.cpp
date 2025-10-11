#include "pch.h"
#include "DX12DynamicRHI.h"
#include "DX12Swapchain.h"
#include "d3d12.h"
#include "GLFW/glfw3native.h"
#include "Rendering/Renderer.h"
#include "Core/Variables.h"

static UINT64 fenceValues[MAX_FRAMES_IN_FLIGHT] = {};

void DX12DynamicRHI::init()
{
	// Factory
	uint32_t flags = 0;

	#ifdef ENABLE_RHI_VALIDATION
	if (engine_rhi_validation)
	{
		ComPtr<ID3D12Debug> dc;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dc))))
		{
			ComPtr<ID3D12Debug6> dc6;
			dc->QueryInterface(IID_PPV_ARGS(&dc6));
			dc6->EnableDebugLayer();
			//dc6->SetEnableGPUBasedValidation(true);
			flags = DXGI_CREATE_FACTORY_DEBUG;
		}
	}
	#endif

	CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory));

	// Adapter (aka Physical Device)
	for (int i = 0; factory->EnumAdapters1(i, &adapter) == S_OK; i++)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		// Skip basic driver
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, __uuidof(ID3D12Device), nullptr)))
			break;

		adapter->Release();
	}

	// Device (aka Logical Device)
	D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&device));

	#ifdef ENABLE_RHI_VALIDATION
	if (engine_rhi_validation)
		device->QueryInterface(IID_PPV_ARGS(&debug_device));
	#endif

	// Allocator
	D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
	allocatorDesc.pDevice = device.Get();
	allocatorDesc.pAdapter = adapter.Get();
	allocatorDesc.Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS;

	D3D12MA::CreateAllocator(&allocatorDesc, &allocator);

	// Command Queues (for submitting groups of commands)
	cmd_queue = new DX12CommandQueue(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		cmd_lists[i] = new DX12CommandList(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
		fenceValues[i] = MAX_FRAMES_IN_FLIGHT - 1 - i;
	}

	cmd_list_copy = new DX12CommandList(device, D3D12_COMMAND_LIST_TYPE_COPY);
	cmd_list_copy->cmd_allocator->SetName(L"cmd_list_copy_cmd_allocator");
	cmd_queue_copy = new DX12CommandQueue(device, D3D12_COMMAND_LIST_TYPE_COPY);

	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxc_utils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc_compiler));

	dxc_utils->CreateDefaultIncludeHandler(&dxc_include_handler);

	// For resources (one for all srv types, because docs says that it will be better)
	cbv_srv_uav_heap = new DX12FrameDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100'000, MAX_BINDLESS_TEXTURES);
	cbv_srv_uav_staging_heap = new DX12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100'000, false);

	// For imgui, reserve first 16 for internal resources like fonts etc
	cbv_srv_uav_additional_heap = new DX12FrameDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1'000, 16);

	// 2048 is maximum for sampler descriptor heap visible for shaders
	// It contains only unique samplers, this should be enough for all use cases
	samplers_heap = new DX12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048, true);

	render_target_view_heap = new DX12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024, false);
	depth_stencil_view_heap = new DX12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1024, false);

	bindless_resources = new DX12BindlessResources();

	tracy_ctx = TracyD3D12Context(device.Get(), cmd_queue->cmd_queue.Get());
}

void DX12DynamicRHI::shutdown()
{
	TracyD3D12Destroy(tracy_ctx);

	release_gpu_resources(UINT64_MAX);
	gDynamicRHI->getBindlessResources()->cleanup();

	auto *bindless = bindless_resources;
	bindless_resources = nullptr;
	buffers_for_shaders.clear();
	delete bindless;

	delete cmd_queue;
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		delete cmd_lists[i];

	delete cmd_queue_copy;
	delete cmd_list_copy;

	if (swapchain)
	{
		swapchain->cleanup();
		swapchain = nullptr;
	}

	delete render_target_view_heap;
	delete depth_stencil_view_heap;

	cached_shaders.clear();

	release_gpu_resources(UINT64_MAX);

	SAFE_RELEASE(allocator);

	delete cbv_srv_uav_heap;
	delete samplers_heap;

	delete cbv_srv_uav_staging_heap;

	delete cbv_srv_uav_additional_heap;

	SAFE_RELEASE(dxc_include_handler);
	SAFE_RELEASE(dxc_compiler);
	SAFE_RELEASE(dxc_utils);

	if (adapter)
		adapter.Reset();
	if (factory)
		factory.Reset();

	#ifdef ENABLE_RHI_VALIDATION
	if (engine_rhi_validation)
	{
		debug_device->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
		if (debug_device)
			debug_device.Reset();
	}
	#endif

	if (device)
		device.Reset();
}

RHISwapchainRef DX12DynamicRHI::createSwapchain(GLFWwindow *window)
{
	HWND hWnd = glfwGetWin32Window(window);
	int width, height;
	glfwGetWindowSize(window, &width, &height);

	SwapchainInfo info;
	info.width = width;
	info.height = height;
	info.format = FORMAT_R8G8B8A8_UNORM;
	info.textures_count = MAX_FRAMES_IN_FLIGHT;
	swapchain = new DX12Swapchain(hWnd, info);
	return swapchain;
}

void DX12DynamicRHI::resizeSwapchain(int width, int height)
{
	gDynamicRHI->waitGPU();
	swapchain->resize(width, height);
}

RHIShaderRef DX12DynamicRHI::createShader(eastl::wstring path, ShaderType type, eastl::wstring entry_point)
{
	if (entry_point.empty())
	{
		if (type == VERTEX_SHADER)
			entry_point = L"VSMain";
		else if (type == FRAGMENT_SHADER)
			entry_point = L"PSMain";
		else if (type == COMPUTE_SHADER)
			entry_point = L"CSMain";
		else if (type == RAY_GENERATION_SHADER)
			entry_point = L"RayGen";
		else if (type == MISS_SHADER)
			entry_point = L"Miss";
		else if (type == CLOSEST_HIT_SHADER)
			entry_point = L"ClosestHit";
	}

	size_t cache_hash = 0;
	hash_combine(cache_hash, path);
	hash_combine(cache_hash, type);
	hash_combine(cache_hash, entry_point);

	if (cached_shaders.find(cache_hash) != cached_shaders.end())
	{
		return cached_shaders[cache_hash];
	}

	auto shader = new DX12Shader(path, type, entry_point, {}, dxc_utils);
	cached_shaders[cache_hash] = shader;
	return shader;
}

RHIShaderRef DX12DynamicRHI::createShader(eastl::wstring path, ShaderType type, eastl::vector<eastl::pair<const char *, const char *>> defines)
{
	eastl::wstring entry_point;
	if (type == VERTEX_SHADER)
		entry_point = L"VSMain";
	else if (type == FRAGMENT_SHADER)
		entry_point = L"PSMain";
	else if (type == COMPUTE_SHADER)
		entry_point = L"CSMain";
	else if (type == RAY_GENERATION_SHADER)
		entry_point = L"RayGen";
	else if (type == MISS_SHADER)
		entry_point = L"Miss";
	else if (type == CLOSEST_HIT_SHADER)
		entry_point = L"ClosestHit";

	size_t cache_hash = 0;
	hash_combine(cache_hash, path);
	hash_combine(cache_hash, type);
	hash_combine(cache_hash, entry_point);

	eastl::string all_defines = "";
	for (const auto &define : defines)
	{
		all_defines += define.first;
		all_defines += define.second;
	}
	Engine::Math::hash_combine(cache_hash, all_defines);

	if (cached_shaders.find(cache_hash) != cached_shaders.end())
	{
		return cached_shaders[cache_hash];
	}

	auto shader = new DX12Shader(path, type, entry_point, defines, dxc_utils);
	cached_shaders[cache_hash] = shader;
	return shader;
}

RHIPipelineRef DX12DynamicRHI::createPipeline()
{
	auto pipeline = new DX12Pipeline(device.Get());
	return pipeline;
}

RHIBufferRef DX12DynamicRHI::createBuffer(BufferDescription description)
{
	auto buffer = new DX12Buffer(description);
	return buffer;
}

RHITextureRef DX12DynamicRHI::createTexture(TextureDescription description)
{
	auto texture = new DX12Texture(this, description);
	return texture;
}

RHIBottomLevelAccelerationStructureRef DX12DynamicRHI::createBottomLevelAccelerationStructure()
{
	return new DX12BottomLevelAccelerationStructure();
}

RHITopLevelAccelerationStructureRef DX12DynamicRHI::createTopLevelAccelerationStructure()
{
	return new DX12TopLevelAccelerationStructure();
}

void DX12DynamicRHI::waitGPU()
{
	const UINT64 current_fence_value = fenceValues[frame_in_flight] + MAX_FRAMES_IN_FLIGHT;
	cmd_queue->signal(current_fence_value);
	cmd_queue->wait(current_fence_value);
}

void DX12DynamicRHI::prepareRenderCall()
{
	PROFILE_CPU_FUNCTION();
	DX12CommandList *cmd_list_native = static_cast<DX12CommandList *>(getCmdList());
	DX12Pipeline *native_pso = static_cast<DX12Pipeline *>(cmd_list_native->current_pipeline);
	bool pso_changed = false;
	if (native_pso != last_native_pso)
	{
		last_native_pso = native_pso;
		pso_changed = true;
	}

	const DX12Shader::BindingInfo &binding_info = native_pso->binding_info;

	DX12Descriptor first_srv_heap_textures_descriptor = {};
	DX12Descriptor first_cbv_heap_uav_textures_descriptor = {};

	if (binding_info.srv_table.registersCount() > 0)
		first_srv_heap_textures_descriptor = cbv_srv_uav_heap->allocate(binding_info.srv_table.registersCount());

	if (binding_info.uav_table.registersCount() > 0)
		first_cbv_heap_uav_textures_descriptor = cbv_srv_uav_heap->allocate(binding_info.uav_table.registersCount());

	if (is_textures_dirty)
	{
		for (int i = binding_info.srv_table.begin_register; i < binding_info.srv_table.end_register; i++)
		{
			if (current_bind_textures[i] == nullptr)
				continue;

			int relative_index = i - binding_info.srv_table.begin_register;
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_srv_heap(cbv_srv_uav_heap->getHandle(first_srv_heap_textures_descriptor.getIndex() + relative_index).getCpuHandle());

			// Copy from staging heap, to current frame's shader visible heap
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_staging_heap = current_bind_textures_descriptors[i];

			device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, cpu_handle_staging_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
	}

	if (is_acceleration_structures_dirty)
	{
		for (int i = binding_info.srv_table.begin_register; i < binding_info.srv_table.end_register; i++)
		{
			if (current_bind_acceleration_structures[i] == nullptr)
				continue;

			auto as = current_bind_acceleration_structures[i];
			DX12TopLevelAccelerationStructure *native_as = (DX12TopLevelAccelerationStructure *)as;


			int relative_index = i - binding_info.srv_table.begin_register;
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_srv_heap(cbv_srv_uav_heap->getHandle(first_srv_heap_textures_descriptor.getIndex() + relative_index).getCpuHandle());

			// Copy from staging heap, to current frame's shader visible heap
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_staging_heap = native_as->shader_resource_view.getCpuHandle();

			device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, cpu_handle_staging_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
	}

	// UAV
	if (is_uav_textures_dirty)
	{
		for (int i = binding_info.uav_table.begin_register; i < binding_info.uav_table.end_register; i++)
		{
			if (current_bind_uav_textures[i] == nullptr)
				continue;

			int relative_index = i - binding_info.uav_table.begin_register;
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_srv_heap(cbv_srv_uav_heap->getHandle(first_cbv_heap_uav_textures_descriptor.getIndex() + relative_index).getCpuHandle());

			// Copy from staging heap, to current frame's shader visible heap
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_staging_heap = current_bind_uav_textures_descriptors[i];

			device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, cpu_handle_staging_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			DX12Texture *native_uav_texture = (DX12Texture *)current_bind_uav_textures[i];
			// TODO: transit to UAV
			native_uav_texture->transitLayout(cmd_list_native, TEXTURE_LAYOUT_UAV);
		}
	}

	// Constant buffers
	if (pso_changed || is_buffers_dirty)
	{
		for (auto &info : binding_info.constant_buffers)
		{
			if (current_bind_buffers[info.bind_point] == nullptr)
				continue;

			if (native_pso->description.is_compute_pipeline || native_pso->description.is_ray_tracing_pipeline)
				cmd_list_native->cmd_list->SetComputeRootConstantBufferView(info.root_param_index, current_bind_buffers_gpu_address[info.bind_point]);
			else
				cmd_list_native->cmd_list->SetGraphicsRootConstantBufferView(info.root_param_index, current_bind_buffers_gpu_address[info.bind_point]);
		}
	}


	// Set where descriptor table will start getting data from heap.
	
	bool is_compute_pipeline = native_pso->description.is_compute_pipeline || native_pso->description.is_ray_tracing_pipeline;
	auto setRootDescriptorTable = [is_compute_pipeline, cmd_list_native](uint32_t root_parameter_index, D3D12_GPU_DESCRIPTOR_HANDLE start_descriptor)
	{
		if (is_compute_pipeline)
			cmd_list_native->cmd_list->SetComputeRootDescriptorTable(root_parameter_index, start_descriptor);
		else
			cmd_list_native->cmd_list->SetGraphicsRootDescriptorTable(root_parameter_index, start_descriptor);
	};

	bool is_update_srv_table = pso_changed || is_textures_dirty || is_acceleration_structures_dirty;
	bool is_update_uav = pso_changed || is_uav_textures_dirty;
	if (binding_info.srv_table.registersCount() > 0 && is_update_srv_table)
		setRootDescriptorTable(binding_info.srv_table.table_index, first_srv_heap_textures_descriptor.getGpuHandle());

	if (binding_info.uav_table.table_index != -1 && is_uav_textures_dirty)
		setRootDescriptorTable(binding_info.uav_table.table_index, first_cbv_heap_uav_textures_descriptor.getGpuHandle());


	// Bindless
	{
		D3D12_GPU_DESCRIPTOR_HANDLE srv_bindless_gpu_handle(cbv_srv_uav_heap->getHandle(0).getGpuHandle());
		D3D12_GPU_DESCRIPTOR_HANDLE sampler_bindless_gpu_handle(samplers_heap->getHandle(0).getGpuHandle());

		setRootDescriptorTable(binding_info.srv_bindless, srv_bindless_gpu_handle);
		setRootDescriptorTable(binding_info.samplers_bindless, sampler_bindless_gpu_handle);
	}

	is_textures_dirty = false;
	is_uav_textures_dirty = false;
	is_buffers_dirty = false;
	is_acceleration_structures_dirty = false;
}

void DX12DynamicRHI::beginFrame()
{
	PROFILE_CPU_FUNCTION();
	for (auto &tex : current_bind_textures)
		tex = nullptr;
	for (auto &buf : current_bind_buffers)
		buf = nullptr;
	for (auto &tex : current_bind_uav_textures)
		tex = nullptr;
	for (auto &as : current_bind_acceleration_structures)
		as = nullptr;

	image_index = swapchain->swap_chain->GetCurrentBackBufferIndex();
	Renderer::beginFrame();

	TracyD3D12NewFrame(tracy_ctx);
	TracyD3D12Collect(tracy_ctx);

	// Reset offsets for uniform buffers
	for (auto &buffers : buffers_for_shaders)
	{
		buffers.second.current_offset = 0;
	}

	// Command list is already sent to execution, so after ExecuteCommandList we can reset it at any time (thats why only one will be enough)
	getCmdList()->open();

	ID3D12DescriptorHeap *heaps[] = { cbv_srv_uav_heap->getHeap(), samplers_heap->getHeap() };

	cmd_lists[frame_in_flight]->cmd_list->SetDescriptorHeaps(_countof(heaps), heaps); // do it when open cmd list?
	cbv_srv_uav_heap->releaseFrame(fenceValues[frame_in_flight]);
	cbv_srv_uav_additional_heap->releaseFrame(fenceValues[frame_in_flight]);
}

void DX12DynamicRHI::endFrame()
{
	PROFILE_CPU_FUNCTION();
	gDynamicRHI->getCmdList()->close();

	cmd_queue->execute(cmd_lists[frame_in_flight]);

	swapchain->swap_chain->Present(render_vsync ? 1 : 0, 0);

	// Set current fence value on current frame completion
	const UINT64 current_fence_value = fenceValues[frame_in_flight];
	cmd_queue->signal(current_fence_value);

	// Wait for previous frame to complete
	frame_in_flight = (frame_in_flight + 1) % MAX_FRAMES_IN_FLIGHT;

	{
		PROFILE_CPU_SCOPE("Wait in flight fence");
		const UINT64 prev_fence_value = fenceValues[frame_in_flight];
		cmd_queue->wait(prev_fence_value);
	}

	// Set the fence value for the next frame.
	fenceValues[frame_in_flight] = current_fence_value + 1;

	cbv_srv_uav_heap->finishFrame(frame);
	cbv_srv_uav_additional_heap->finishFrame(frame);
	release_gpu_resources(frame);
	frame++;
}
