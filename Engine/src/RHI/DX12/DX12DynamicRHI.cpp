#include "pch.h"
#include "DX12DynamicRHI.h"
#include "DX12Swapchain.h"
#include "d3d12.h"
#include "GLFW/glfw3native.h"
#include "Rendering/Renderer.h"
#include "Core/Variables.h"
#include "RHI/StreamlineWrapper.h"

static void validation_callback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR description, void *context)
{
	if (severity <= D3D12_MESSAGE_SEVERITY_ERROR)
	{
		CORE_ERROR("{}", description);
	} else if (severity == D3D12_MESSAGE_SEVERITY_WARNING)
	{
		CORE_WARN("{}", description);
	}
}

void DX12DynamicRHI::init()
{
	if (engine_streamline)
		StreamlineWrapper::init();

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

		/*
		ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
		D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings));

		dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		*/
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
	{
		device->QueryInterface(IID_PPV_ARGS(&debug_device));

		ComPtr<ID3D12InfoQueue1> info_queue;
		device->QueryInterface(IID_PPV_ARGS(&info_queue));

		D3D12_MESSAGE_ID deny_ids[] = {D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE};
		D3D12_MESSAGE_SEVERITY deny_severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumSeverities = _countof(deny_severities);
		filter.DenyList.pSeverityList = deny_severities;
		filter.DenyList.NumIDs = _countof(deny_ids);
		filter.DenyList.pIDList = deny_ids;
		info_queue->PushStorageFilter(&filter);

		DWORD callback_cookie;
		info_queue->RegisterMessageCallback(validation_callback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &callback_cookie);

		if (engine_rhi_validation_break)
		{
			info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
		}
	}
	#endif

	if (engine_streamline)
	{
		streamline.init();
		dlss_upscaler.init();
	}

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
		upload_cmd_lists[i] = new DX12CommandList(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}

	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxc_utils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc_compiler));

	dxc_utils->CreateDefaultIncludeHandler(&dxc_include_handler);

	// For resources (one for all srv types, because docs says that it will be better)
	cbv_srv_uav_heap = new DX12FrameDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100'000, MAX_BINDLESS_RESOURCES);
	cbv_srv_uav_staging_heap = new DX12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100'000, false);

	// For imgui, reserve first 16 for internal resources like fonts etc
	cbv_srv_uav_additional_heap = new DX12FrameDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1'000, 16);

	// 2048 is maximum for sampler descriptor heap visible for shaders
	// It contains only unique samplers, this should be enough for all use cases
	samplers_heap = new DX12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048, true);

	render_target_view_heap = new DX12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024, false);
	depth_stencil_view_heap = new DX12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1024, false);

	bindless_resources = new DX12BindlessResources();
	bindless_resources->init();

	tracy_ctx = TracyD3D12Context(device.Get(), cmd_queue->cmd_queue.Get());

	D3D12_INDIRECT_ARGUMENT_DESC indirect_arguments[1];
	D3D12_COMMAND_SIGNATURE_DESC command_signature_desc{};

	indirect_arguments[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	command_signature_desc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
	command_signature_desc.NumArgumentDescs = 1;
	command_signature_desc.pArgumentDescs = indirect_arguments;
	device->CreateCommandSignature(&command_signature_desc, nullptr, IID_PPV_ARGS(&draw_indexed_command_signature));

	indirect_arguments[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	command_signature_desc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
	command_signature_desc.NumArgumentDescs = 1;
	command_signature_desc.pArgumentDescs = indirect_arguments;
	device->CreateCommandSignature(&command_signature_desc, nullptr, IID_PPV_ARGS(&draw_command_signature));

	indirect_arguments[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
	command_signature_desc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
	command_signature_desc.NumArgumentDescs = 1;
	command_signature_desc.pArgumentDescs = indirect_arguments;
	device->CreateCommandSignature(&command_signature_desc, nullptr, IID_PPV_ARGS(&dispatch_command_signature));

	indirect_arguments[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
	command_signature_desc.ByteStride = sizeof(D3D12_DISPATCH_MESH_ARGUMENTS);
	command_signature_desc.NumArgumentDescs = 1;
	command_signature_desc.pArgumentDescs = indirect_arguments;
	device->CreateCommandSignature(&command_signature_desc, nullptr, IID_PPV_ARGS(&dispatch_mesh_command_signature));

	// Create pipeline statistics
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		D3D12_QUERY_HEAP_DESC query_heap_desc{};
		query_heap_desc.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS1;
		query_heap_desc.Count = 1;
		device->CreateQueryHeap(&query_heap_desc, IID_PPV_ARGS(&pipeline_statistics_queries[i].query_heap));

		D3D12_HEAP_PROPERTIES heap_props{};
		heap_props.Type = D3D12_HEAP_TYPE_READBACK;

		D3D12_RESOURCE_DESC resource_desc{};
		resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resource_desc.Width = sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS1);
		resource_desc.Height = 1;
		resource_desc.DepthOrArraySize = 1;
		resource_desc.MipLevels = 1;
		resource_desc.SampleDesc.Count = 1;
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		device->CreateCommittedResource(
			&heap_props,
			D3D12_HEAP_FLAG_NONE,
			&resource_desc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&pipeline_statistics_queries[i].readback_buffer)
		);
	}
}

void DX12DynamicRHI::shutdown()
{
	TracyD3D12Destroy(tracy_ctx);
	dlss_upscaler.shutdown();
	StreamlineWrapper::shutdown();

	release_gpu_resources(UINT64_MAX);
	gDynamicRHI->getBindlessResources()->cleanup();

	auto *bindless = bindless_resources;
	bindless_resources = nullptr;
	buffers_for_shaders.clear();
	delete bindless;

	delete cmd_queue;
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		delete cmd_lists[i];
		delete upload_cmd_lists[i];
	}

	if (swapchain)
	{
		swapchain->cleanup();
		swapchain = nullptr;
	}

	delete render_target_view_heap;
	delete depth_stencil_view_heap;

	cached_shaders.clear();

	SAFE_RELEASE(draw_indexed_command_signature);
	SAFE_RELEASE(draw_command_signature);
	SAFE_RELEASE(dispatch_command_signature);
	SAFE_RELEASE(dispatch_mesh_command_signature);
	for (auto &queries : pipeline_statistics_queries)
		queries = {};

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

	SwapchainDescription info;
	info.width = width;
	info.height = height;
	info.format = Format::R8G8B8A8_UNORM;
	info.texture_count = MAX_FRAMES_IN_FLIGHT;
	swapchain = new DX12Swapchain(hWnd, info);
	return swapchain;
}

void DX12DynamicRHI::resizeSwapchain(int width, int height)
{
	swapchain->resize(width, height);
}

RHIShader *DX12DynamicRHI::create_shader(eastl::wstring path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines)
{
	return new DX12Shader(path, type, entry_point, defines, dxc_utils);
}

RHIBuffer *DX12DynamicRHI::create_buffer(const BufferDescription &description)
{
	return new DX12Buffer(description);
}

RHIPipelineRef DX12DynamicRHI::createPipeline(const PipelineDescription &description)
{
	return new DX12Pipeline(device.Get(), description);
}

RHITextureRef DX12DynamicRHI::createTexture(const TextureDescription &description, const void *initial_data)
{
	return new DX12Texture(description, initial_data);
}

RHIBottomLevelAccelerationStructureRef DX12DynamicRHI::createBottomLevelAccelerationStructure()
{
	return new DX12BottomLevelAccelerationStructure();
}

RHITopLevelAccelerationStructureRef DX12DynamicRHI::createTopLevelAccelerationStructure()
{
	return new DX12TopLevelAccelerationStructure();
}

RHICommandList *DX12DynamicRHI::getUploadCmdList()
{
	DX12CommandList *cmd_list = upload_cmd_lists[frame_in_flight];
	cmd_list->open();
	return cmd_list;
}

void DX12DynamicRHI::waitGPU()
{
	PROFILE_CPU_FUNCTION();
	cmd_queue->waitIdle();
}

void DX12DynamicRHI::prepareRenderCall()
{
	PROFILE_CPU_FUNCTION();
	DX12CommandList *cmd_list_native = static_cast<DX12CommandList *>(getCmdList());
	DX12Pipeline *native_pso = static_cast<DX12Pipeline *>(cmd_list_native->getCurrentPipeline());
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

	bool is_compute_pipeline = native_pso->description.pipeline_type == PipelineType::COMPUTE || native_pso->description.pipeline_type == PipelineType::RAY_TRACING;

	// Constant buffers
	if (pso_changed || is_buffers_dirty)
	{
		for (auto &info : binding_info.constant_buffers)
		{
			if (current_bind_buffers[info.bind_point] == nullptr)
				continue;

			if (is_compute_pipeline)
				cmd_list_native->cmd_list->SetComputeRootConstantBufferView(info.root_param_index, current_bind_buffers[info.bind_point]->getGPUAddress());
			else
				cmd_list_native->cmd_list->SetGraphicsRootConstantBufferView(info.root_param_index, current_bind_buffers[info.bind_point]->getGPUAddress());
		}
	}


	// Set where descriptor table will start getting data from heap.
	auto setRootDescriptorTable = [is_compute_pipeline, cmd_list_native](uint32_t root_parameter_index, D3D12_GPU_DESCRIPTOR_HANDLE start_descriptor)
	{
		if (is_compute_pipeline)
			cmd_list_native->cmd_list->SetComputeRootDescriptorTable(root_parameter_index, start_descriptor);
		else
			cmd_list_native->cmd_list->SetGraphicsRootDescriptorTable(root_parameter_index, start_descriptor);
	};

	bool is_update_srv_table = pso_changed;
	if (binding_info.srv_table.registersCount() > 0 && is_update_srv_table)
		setRootDescriptorTable(binding_info.srv_table.table_index, first_srv_heap_textures_descriptor.getGpuHandle());

	// Bindless
	{
		D3D12_GPU_DESCRIPTOR_HANDLE srv_bindless_gpu_handle(cbv_srv_uav_heap->getHandle(0).getGpuHandle());
		D3D12_GPU_DESCRIPTOR_HANDLE sampler_bindless_gpu_handle(samplers_heap->getHandle(0).getGpuHandle());

		setRootDescriptorTable(binding_info.srv_bindless, srv_bindless_gpu_handle);
		setRootDescriptorTable(binding_info.samplers_bindless, sampler_bindless_gpu_handle);
	}

	is_buffers_dirty = false;
}

void DX12DynamicRHI::bindDescriptorHeaps()
{
	ID3D12DescriptorHeap *heaps[] = { cbv_srv_uav_heap->getHeap(), samplers_heap->getHeap() };
	cmd_lists[frame_in_flight]->cmd_list->SetDescriptorHeaps(_countof(heaps), heaps);
}

void DX12DynamicRHI::beginFrame()
{
	PROFILE_CPU_FUNCTION();
	for (auto &buf : current_bind_buffers)
		buf = nullptr;

	swapchain->acquire();
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

	bindDescriptorHeaps();
	if (frame >= MAX_FRAMES_IN_FLIGHT)
	{
		cbv_srv_uav_heap->releaseFrame(frame - MAX_FRAMES_IN_FLIGHT);
		cbv_srv_uav_additional_heap->releaseFrame(frame - MAX_FRAMES_IN_FLIGHT);
	}

	cmd_lists[frame_in_flight]->cmd_list->BeginQuery(pipeline_statistics_queries[frame_in_flight].query_heap.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS1, 0);
}

void DX12DynamicRHI::endFrame()
{
	PROFILE_CPU_FUNCTION();

	cmd_lists[frame_in_flight]->cmd_list->EndQuery(pipeline_statistics_queries[frame_in_flight].query_heap.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS1, 0);
	cmd_lists[frame_in_flight]->cmd_list->ResolveQueryData(pipeline_statistics_queries[frame_in_flight].query_heap.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS1, 0, 1, pipeline_statistics_queries[frame_in_flight].readback_buffer.Get(), 0);

	gDynamicRHI->getCmdList()->close();

	eastl::vector<RHICommandList *> execute_cmd_lists;
	if (upload_cmd_lists[frame_in_flight]->isOpen())
	{
		upload_cmd_lists[frame_in_flight]->close();
		execute_cmd_lists.push_back(upload_cmd_lists[frame_in_flight]);
	}
	execute_cmd_lists.push_back(cmd_lists[frame_in_flight]);
	uint64_t frame_fence_value = cmd_queue->execute(execute_cmd_lists);

	swapchain->present();

	advance_frame_in_flight(frame_fence_value);

	// Read pipeline statistics
	D3D12_QUERY_DATA_PIPELINE_STATISTICS1 stats;
	D3D12_RANGE read_range = { 0, sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS1) };
	void* mapped_data = nullptr;
	if (SUCCEEDED(pipeline_statistics_queries[frame_in_flight].readback_buffer->Map(0, &read_range, &mapped_data)))
	{
		memcpy(&stats, mapped_data, sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS1));
		D3D12_RANGE write_range = { 0, 0 };
		pipeline_statistics_queries[frame_in_flight].readback_buffer->Unmap(0, &write_range);

		gpu_statistics.input_assembly_vertices = stats.IAVertices;
		gpu_statistics.input_assembly_primitives = stats.IAPrimitives;
		gpu_statistics.vertex_shader_invocations = stats.VSInvocations;
		gpu_statistics.clipping_invocations = stats.CInvocations;
		gpu_statistics.clipping_primitives = stats.CPrimitives;
		gpu_statistics.fragment_shader_invocations = stats.PSInvocations;
		gpu_statistics.compute_shader_invocations = stats.CSInvocations;
		gpu_statistics.mesh_shader_invocations = stats.MSInvocations;
	}

	cbv_srv_uav_heap->finishFrame(frame);
	cbv_srv_uav_additional_heap->finishFrame(frame);
	release_gpu_resources(frame);
	frame++;
}
