#include "pch.h"
#include "DX12DynamicRHI.h"
#include "DX12Swapchain.h"
#include "d3d12.h"
#include "GLFW/glfw3native.h"
#include "Rendering/Renderer.h"
#include "Variables.h"

static UINT64 fenceValues[MAX_FRAMES_IN_FLIGHT] = {};

void DX12DynamicRHI::init()
{
	// Factory
	uint32_t flags = 0;

	#ifdef DEBUG
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

	#ifdef DEBUG
	device->QueryInterface(IID_PPV_ARGS(&debug_device));
	#endif

	// Command Queues (for submitting groups of commands)
	cmd_queue = new DX12CommandQueue(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		cmd_lists[i] = new DX12CommandList(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
		fenceValues[i] = MAX_FRAMES_IN_FLIGHT - 1 - i;
	}

	cmd_list_copy = new DX12CommandList(device, D3D12_COMMAND_LIST_TYPE_COPY);
	cmd_queue_copy = new DX12CommandQueue(device, D3D12_COMMAND_LIST_TYPE_COPY);

	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxc_utils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc_compiler));

	dxc_utils->CreateDefaultIncludeHandler(&dxc_include_handler);

	cbv_srv_uav_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	sampler_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		// Heaps for resources (one for all srv types, because docs says that it will be better)
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
		heap_desc.NumDescriptors = 100'000; // Adjust as needed
		heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&cbv_srv_uav_heap[i]));
	}

	{
		// Staging resources heap
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
		heap_desc.NumDescriptors = 100'000; // Adjust as needed
		heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&cbv_srv_uav_staging_heap));
		cbv_srv_uav_heap_bindless_start = heap_desc.NumDescriptors / 2; // Bindless resources will be in second part of heap
	}

	{
		// Additional heap (for imgui)
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
		heap_desc.NumDescriptors = 1'000; // Adjust as needed
		heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&cbv_srv_uav_additional_heap));
	}

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		// Heaps for samplers
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
		heap_desc.NumDescriptors = 2048; // 2048 is maximum for sampler descriptor heap visible for shaders
		heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&samplers_heap[i]));
	}
	{
		// Staging samplers heap
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
		heap_desc.NumDescriptors = 2048; // Adjust as needed
		heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&samplers_staging_heap));
		sampler_heap_bindless_start = heap_desc.NumDescriptors / 2; // Bindless resources will be in second part of heap
	}

	// Describe and create a render target view (RTV) descriptor heap.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = 1000;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&render_target_view_heap));

	rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// DSV heap
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1000;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsv_heap));

	dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);


	bindless_resources = new DX12BindlessResources();
	bindless_resources->init();

	tracy_ctx = TracyD3D12Context(device.Get(), cmd_queue->cmd_queue.Get());
}

void DX12DynamicRHI::shutdown()
{
	TracyD3D12Destroy(tracy_ctx);

	auto *bindless = bindless_resources;
	bindless_resources = nullptr;
	buffers_for_shaders.clear();
	delete bindless;
	
	delete cmd_queue;
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		delete cmd_lists[i];
	}

	delete cmd_queue_copy;
	delete cmd_list_copy;

	if (swapchain)
	{
		swapchain->cleanup();
		swapchain = nullptr;
	}

	if (render_target_view_heap) {
		render_target_view_heap.Reset();
	}
	if (dsv_heap) {
		dsv_heap.Reset();
	}

	cached_shaders.clear();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		releaseGPUResources(i);
	}

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (cbv_srv_uav_heap[i]) {
			SAFE_RELEASE(cbv_srv_uav_heap[i]);
		}
		if (samplers_heap[i]) {
			SAFE_RELEASE(samplers_heap[i]);
		}
	}
	if (cbv_srv_uav_staging_heap) {
		SAFE_RELEASE(cbv_srv_uav_staging_heap);
	}
	if (cbv_srv_uav_additional_heap) {
		SAFE_RELEASE(cbv_srv_uav_additional_heap);
	}

	SAFE_RELEASE(samplers_staging_heap);


	SAFE_RELEASE(dxc_include_handler);
	SAFE_RELEASE(dxc_compiler);
	SAFE_RELEASE(dxc_utils);

	if (adapter) {
		adapter.Reset();
	}
	if (factory) {
		factory.Reset();
	}

	#ifdef DEBUG
		debug_device->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
		if (debug_device) {
			debug_device.Reset();
		}
	#endif

	if (device) {
		device.Reset();
	}
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

RHIShaderRef DX12DynamicRHI::createShader(std::wstring path, ShaderType type, std::wstring entry_point)
{
	if (entry_point.empty())
	{
		if (type == VERTEX_SHADER)
			entry_point = L"VSMain";
		else if (type == FRAGMENT_SHADER)
			entry_point = L"PSMain";
		else if (type == COMPUTE_SHADER)
			entry_point = L"CSMain";
	}

	size_t cache_hash = 0;
	hash_combine(cache_hash, path);
	hash_combine(cache_hash, type);
	hash_combine(cache_hash, entry_point);

	if (cached_shaders.find(cache_hash) != cached_shaders.end())
	{
		return cached_shaders[cache_hash];
	}

	size_t hash;
	ComPtr<IDxcBlob> blob = compile_shader(path, type, entry_point, false, hash);

	DxcBuffer reflectionBuffer = {};
	reflectionBuffer.Ptr = blob->GetBufferPointer();
	reflectionBuffer.Size = blob->GetBufferSize();
	reflectionBuffer.Encoding = 0;

	ComPtr<ID3D12ShaderReflection> reflection;
	dxc_utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&reflection));
	auto shader = new DX12Shader(blob, reflection, type, hash);
	cached_shaders[cache_hash] = shader;
	return shader;
}

RHIShaderRef DX12DynamicRHI::createShader(std::wstring path, ShaderType type, std::vector<std::pair<const char *, const char *>> defines)
{
	std::wstring entry_point;
	if (type == VERTEX_SHADER)
		entry_point = L"VSMain";
	else if (type == FRAGMENT_SHADER)
		entry_point = L"PSMain";
	else if (type == COMPUTE_SHADER)
		entry_point = L"CSMain";

	size_t cache_hash = 0;
	hash_combine(cache_hash, path);
	hash_combine(cache_hash, type);
	hash_combine(cache_hash, entry_point);

	std::string all_defines = "";
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

	size_t hash;
	ComPtr<IDxcBlob> blob = compile_shader(path, type, entry_point, false, hash, &defines);

	DxcBuffer reflectionBuffer = {};
	reflectionBuffer.Ptr = blob->GetBufferPointer();
	reflectionBuffer.Size = blob->GetBufferSize();
	reflectionBuffer.Encoding = 0;

	ComPtr<ID3D12ShaderReflection> reflection;
	dxc_utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&reflection));
	auto shader = new DX12Shader(blob, reflection, type, hash);
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

void DX12DynamicRHI::waitGPU()
{
	const UINT64 current_fence_value = fenceValues[current_frame] + MAX_FRAMES_IN_FLIGHT;
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

	// Copy binded textures descriptors to heap
	if (is_textures_dirty)
	{
		last_cbv_heap_textures_offset = cbv_srv_uav_heap_offset;
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle_srv_heap(cbv_srv_uav_heap[current_frame]->GetCPUDescriptorHandleForHeapStart());
			CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle_sampler_heap(samplers_heap[current_frame]->GetCPUDescriptorHandleForHeapStart());

			cbv_srv_uav_heap_offset += binding_info.srv_table.registersCount();

			cpu_handle_srv_heap.Offset(last_cbv_heap_textures_offset, cbv_srv_uav_descriptor_size);
			cpu_handle_sampler_heap.Offset(last_cbv_heap_textures_offset, sampler_descriptor_size);

			for (int i = binding_info.srv_table.begin_register; i < binding_info.srv_table.end_register; i++)
			{
				if (current_bind_textures[i] == nullptr)
					continue;

				// Copy from staging heap, to current frame's shader visible heap
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_staging_heap = current_bind_textures_descriptors[i];


				//cpu_handle_current_heap.Offset(heap_offset, cbv_srv_uav_descriptor_size);

				device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, cpu_handle_staging_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cpu_handle_srv_heap.Offset(1, cbv_srv_uav_descriptor_size);

				// Sampler
				device->CopyDescriptorsSimple(1, cpu_handle_sampler_heap, current_bind_textures_samplers[i], D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
				cpu_handle_sampler_heap.Offset(1, sampler_descriptor_size);
			}
		}
	}

	// UAV
	if (is_uav_textures_dirty)
	{
		last_cbv_heap_uav_textures_offset = cbv_srv_uav_heap_offset;
		if (binding_info.uav_table.table_index != -1)
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle_srv_heap(cbv_srv_uav_heap[current_frame]->GetCPUDescriptorHandleForHeapStart());

			cbv_srv_uav_heap_offset += binding_info.uav_table.registersCount();

			cpu_handle_srv_heap.Offset(last_cbv_heap_uav_textures_offset, cbv_srv_uav_descriptor_size);

			for (int i = binding_info.uav_table.begin_register; i < binding_info.uav_table.end_register; i++)
			{
				if (current_bind_uav_textures[i] == nullptr)
					continue;

				// Copy from staging heap, to current frame's shader visible heap
				D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_staging_heap = current_bind_uav_textures_descriptors[i];

				device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, cpu_handle_staging_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				cpu_handle_srv_heap.Offset(1, cbv_srv_uav_descriptor_size);
			}
		}
	}

	// Constant buffers
	if (pso_changed || is_buffers_dirty)
	{
		for (auto &info : binding_info.constant_buffers)
		{
			if (current_bind_buffers[info.bind_point] == nullptr)
				continue;

			if (native_pso->description.is_compute_pipeline)
				cmd_list_native->cmd_list->SetComputeRootConstantBufferView(info.root_param_index, current_bind_buffers_gpu_address[info.bind_point]);
			else
				cmd_list_native->cmd_list->SetGraphicsRootConstantBufferView(info.root_param_index, current_bind_buffers_gpu_address[info.bind_point]);
		}
	}


	// Set where descriptor table will start getting data from heap.
	// If there are different drawcalls, then every draw call's gpu handle in heap should be offseted
	CD3DX12_GPU_DESCRIPTOR_HANDLE srv_gpu_handle(cbv_srv_uav_heap[current_frame]->GetGPUDescriptorHandleForHeapStart());
	srv_gpu_handle.Offset(last_cbv_heap_textures_offset, cbv_srv_uav_descriptor_size);

	CD3DX12_GPU_DESCRIPTOR_HANDLE sampler_gpu_handle(samplers_heap[current_frame]->GetGPUDescriptorHandleForHeapStart());
	sampler_gpu_handle.Offset(last_cbv_heap_textures_offset, sampler_descriptor_size);

	CD3DX12_GPU_DESCRIPTOR_HANDLE uav_gpu_handle(cbv_srv_uav_heap[current_frame]->GetGPUDescriptorHandleForHeapStart());
	uav_gpu_handle.Offset(last_cbv_heap_uav_textures_offset, cbv_srv_uav_descriptor_size);


	bool is_update_textures = pso_changed || is_textures_dirty;
	bool is_update_uav = pso_changed || is_uav_textures_dirty;
	if (native_pso->description.is_compute_pipeline)
	{
		// offset?
		if (binding_info.srv_table.table_index != -1 && is_update_textures)
			cmd_list_native->cmd_list->SetComputeRootDescriptorTable(binding_info.srv_table.table_index, srv_gpu_handle);

		// offset?
		if (binding_info.samplers_table.table_index != -1 && is_update_textures)
			cmd_list_native->cmd_list->SetComputeRootDescriptorTable(binding_info.samplers_table.table_index, sampler_gpu_handle);

		if (binding_info.uav_table.table_index != -1 && is_update_uav)
			cmd_list_native->cmd_list->SetComputeRootDescriptorTable(binding_info.uav_table.table_index, uav_gpu_handle);
	} else
	{
		// offset?
		if (binding_info.srv_table.table_index != -1 && is_update_textures)
			cmd_list_native->cmd_list->SetGraphicsRootDescriptorTable(binding_info.srv_table.table_index, srv_gpu_handle);

		// offset?
		if (binding_info.samplers_table.table_index != -1 && is_update_textures)
			cmd_list_native->cmd_list->SetGraphicsRootDescriptorTable(binding_info.samplers_table.table_index, sampler_gpu_handle);

		if (binding_info.uav_table.table_index != -1 && is_update_uav)
			cmd_list_native->cmd_list->SetGraphicsRootDescriptorTable(binding_info.uav_table.table_index, uav_gpu_handle);
	}


	// Bindless
	{
		CD3DX12_GPU_DESCRIPTOR_HANDLE srv_bindless_gpu_handle(cbv_srv_uav_heap[current_frame]->GetGPUDescriptorHandleForHeapStart());
		srv_bindless_gpu_handle.Offset(cbv_srv_uav_heap_bindless_start, cbv_srv_uav_descriptor_size);

		CD3DX12_GPU_DESCRIPTOR_HANDLE sampler_bindless_gpu_handle(samplers_heap[current_frame]->GetGPUDescriptorHandleForHeapStart());
		sampler_bindless_gpu_handle.Offset(sampler_heap_bindless_start, sampler_descriptor_size);

		if (native_pso->description.is_compute_pipeline)
		{
			cmd_list_native->cmd_list->SetComputeRootDescriptorTable(binding_info.srv_bindless, srv_bindless_gpu_handle);
			cmd_list_native->cmd_list->SetComputeRootDescriptorTable(binding_info.samplers_bindless, sampler_bindless_gpu_handle);
		} else
		{
			cmd_list_native->cmd_list->SetGraphicsRootDescriptorTable(binding_info.srv_bindless, srv_bindless_gpu_handle);
			cmd_list_native->cmd_list->SetGraphicsRootDescriptorTable(binding_info.samplers_bindless, sampler_bindless_gpu_handle);
		}
	}

	if (is_textures_dirty)
		is_textures_dirty = false;
	if (is_uav_textures_dirty)
		is_uav_textures_dirty = false;
	if (is_buffers_dirty)
		is_buffers_dirty = false;
}

void DX12DynamicRHI::beginFrame()
{
	PROFILE_CPU_FUNCTION();
	for (auto &tex : current_bind_textures)
		tex = nullptr;
	for (auto &buf : current_bind_buffers)
		buf = nullptr;

	image_index = swapchain->swap_chain->GetCurrentBackBufferIndex();
	Renderer::beginFrame(current_frame, image_index);

	TracyD3D12NewFrame(tracy_ctx);
	TracyD3D12Collect(tracy_ctx);

	// Reset offsets for uniform buffers
	for (auto &buffers : buffers_for_shaders)
	{
		buffers.second.current_offset = 0;
	}

	// Command list is already sent to execution, so after ExecuteCommandList we can reset it at any time (thats why only one will be enough)
	getCmdList()->open();

	ID3D12DescriptorHeap *heaps[] = { cbv_srv_uav_heap[current_frame], samplers_heap[current_frame] };

	cmd_lists[current_frame]->cmd_list->SetDescriptorHeaps(_countof(heaps), heaps); // do it when open cmd list?
	cbv_srv_uav_heap_offset = 0; // fill current frame heap from start

	cbv_srv_uav_heap_additional_heap_offset = 0;

	samplers_heap_offset = 0;
}

void DX12DynamicRHI::endFrame()
{
	PROFILE_CPU_FUNCTION();
	gDynamicRHI->getCmdList()->close();

	cmd_queue->execute(cmd_lists[current_frame]);

	swapchain->swap_chain->Present(render_vsync ? 1 : 0, 0);

	// Set current fence value on current frame completion
	const UINT64 current_fence_value = fenceValues[current_frame];
	cmd_queue->signal(current_fence_value);

	// Wait for previous frame to complete
	current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

	{
		PROFILE_CPU_SCOPE("Wait in flight fence");
		const UINT64 prev_fence_value = fenceValues[current_frame];
		cmd_queue->wait(prev_fence_value);
	}

	// Set the fence value for the next frame.
	fenceValues[current_frame] = current_fence_value + 1;

	releaseGPUResources(current_frame);
}

void DX12DynamicRHI::releaseGPUResources(uint32_t current_frame)
{
	if (!gpu_release_queue[current_frame].empty())
	{
		//vkDeviceWaitIdle(VkWrapper::device->logicalHandle);

		for (auto resource : gpu_release_queue[current_frame])
		{
			switch (resource.first)
			{
				case RESOURCE_TYPE_SAMPLER: 
					break;
				case RESOURCE_TYPE_IMAGE_VIEW:
					break;
				case RESOURCE_TYPE_IMAGE:
				{
					ID3D12Resource *native_resource = (ID3D12Resource *)resource.second;
					native_resource->Release();
					break;
				}
				case RESOURCE_TYPE_MEMORY:
					break;
				case RESOURCE_TYPE_BUFFER:
				{
					ID3D12Resource *native_resource = (ID3D12Resource *)resource.second;
					native_resource->Release();
					break;
				}
				case RESOURCE_TYPE_PIPELINE:
				{
					ID3D12PipelineState *native_resource = (ID3D12PipelineState *)resource.second;
					native_resource->Release();
					break;
				}
				case RESOURCE_TYPE_ROOT_SIGNATURE:
				{
					ID3D12RootSignature *native_resource = (ID3D12RootSignature *)resource.second;
					native_resource->Release();
					break;
				}
			}
		}

		gpu_release_queue[current_frame].clear();
	}
}

