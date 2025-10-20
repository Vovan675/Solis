#include "pch.h"
#include "DX12Buffer.h"
#include "DX12DynamicRHI.h"
#include "DX12Utils.h"

DX12Buffer::DX12Buffer(BufferDescription description) : RHIBuffer(description)
{
	auto *native_rhi = DX12Utils::getNativeRHI();

	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_NONE;

	if (description.usage & UAV_BUFFER)
	{
		resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(description.size, resource_flags);
	D3D12_RESOURCE_STATES resource_state = description.useStagingBuffer ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_GENERIC_READ;
	current_state = description.useStagingBuffer ? RESOURCE_STATE_COMMON : RESOURCE_STATE_COPY_SRC;

	if (description.usage & ACCELERATION_STRUCTURE_STORAGE_BUFFER)
	{
		resource_state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	}

	D3D12MA::ALLOCATION_DESC allocation_desc = {};
	allocation_desc.HeapType = description.useStagingBuffer ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD;

	allocation = std::make_unique<DX12AllocationResource>();
	resource = std::make_unique<DX12Resource>();

	HRESULT res = native_rhi->allocator->CreateResource(
		&allocation_desc,
		&resource_desc,
		resource_state,
		nullptr,
		&allocation->resource,
		IID_PPV_ARGS(&resource->resource));
	assert(res == S_OK);
	//setState(RESOURCE_STATE_VERTEX_BUFFER);

	if (description.usage & STORAGE_BUFFER)
	{
		DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

		// Allocate in staging heap
		shader_resource_view = rhi->cbv_srv_uav_staging_heap->allocate();

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.Format = DXGI_FORMAT_UNKNOWN;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srv_desc.Buffer.FirstElement = 0;
		srv_desc.Buffer.NumElements = description.size / description.stride;
		srv_desc.Buffer.StructureByteStride = description.stride;
		srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		rhi->device->CreateShaderResourceView(resource->resource, &srv_desc, shader_resource_view.getCpuHandle());
	}
}

DX12Buffer::~DX12Buffer()
{
	destroy();
}

void DX12Buffer::destroy()
{
	auto *native_rhi = DX12Utils::getNativeRHI();
	native_rhi->releaseGPUResource(resource.release());
	native_rhi->releaseGPUResource(allocation.release());
}

void DX12Buffer::fill(const void *sourceData)
{
	if (!sourceData)
		return;
	PROFILE_CPU_FUNCTION();

	auto *native_rhi = DX12Utils::getNativeRHI();
	uint64_t buffer_size = description.size;

	if (description.useStagingBuffer)
	{
		ComPtr<ID3D12Resource> intermediate_resource;
		native_rhi->device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(buffer_size),
										D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediate_resource));
		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = sourceData;
		subresourceData.RowPitch = buffer_size;
		subresourceData.SlicePitch = subresourceData.RowPitch;


		RHICommandList *copy_cmd_list = native_rhi->getCmdListCopy();
		// Upload buffer data.
		copy_cmd_list->open();

		UpdateSubresources(native_rhi->cmd_list_copy->cmd_list.Get(),
						   resource->resource, intermediate_resource.Get(),
						   0, 0, 1, &subresourceData);

		copy_cmd_list->close();
		native_rhi->getCmdQueueCopy()->execute(copy_cmd_list);

		// Wait queue
		auto last_fence = native_rhi->getCmdQueueCopy()->getLastFenceValue();
		native_rhi->getCmdQueueCopy()->signal(last_fence + 1);
		native_rhi->getCmdQueueCopy()->wait(last_fence + 1);

		// According to https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#implicit-state-transitions
		current_state = RESOURCE_STATE_SHADER_RESOURCE;

		intermediate_resource.Reset();
	} else
	{
		// For CPU-visible (upload) buffers, map and write directly.
		void* data = nullptr;
		CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
		resource->resource->Map(0, &readRange, &data);
		memcpy(data, sourceData, buffer_size);
		resource->resource->Unmap(0, nullptr);
	}
}

void DX12Buffer::map(void **data)
{
	resource->resource->Map(0, nullptr, data);
}

void DX12Buffer::unmap()
{
	resource->resource->Unmap(0, nullptr);
}

void DX12Buffer::setDebugName(const char *name)
{
	wchar_t wbuf[1024];
	size_t l = std::min(size_t(1023), strlen(name));
	wbuf[l] = '\0';
	mbstowcs(wbuf, name, l);

	resource->resource->SetName(wbuf);
}

void DX12Buffer::setState(ResourceState new_state)
{
	if (current_state == new_state && (new_state & RESOURCE_STATE_UAV) == 0)
		return;

	if (current_state != new_state)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource->resource;
		barrier.Transition.StateBefore = toDX12ResourceState(current_state);
		barrier.Transition.StateAfter = toDX12ResourceState(new_state);
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		DX12CommandList *native_cmd_list = (DX12CommandList *)gDynamicRHI->getCmdList();
		native_cmd_list->cmd_list->ResourceBarrier(1, &barrier);
	} else if ((new_state & RESOURCE_STATE_UAV) != 0)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = resource->resource;

		DX12CommandList *native_cmd_list = (DX12CommandList *)gDynamicRHI->getCmdList();
		native_cmd_list->cmd_list->ResourceBarrier(1, &barrier);
	}

	current_state = new_state;
}
