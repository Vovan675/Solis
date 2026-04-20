#include "pch.h"
#include "DX12Buffer.h"
#include "DX12DynamicRHI.h"
#include "DX12Utils.h"

static D3D12_RESOURCE_STATES toDX12ResourceState(ResourceState state)
{
	D3D12_RESOURCE_STATES result = D3D12_RESOURCE_STATE_COMMON; // = 0
	if (hasAnyFlags(state, ResourceState::SHADER_RESOURCE)) result |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	if (hasAnyFlags(state, ResourceState::COPY_SRC)) result |= D3D12_RESOURCE_STATE_COPY_SOURCE;
	if (hasAnyFlags(state, ResourceState::COPY_DST)) result |= D3D12_RESOURCE_STATE_COPY_DEST;
	if (hasAnyFlags(state, ResourceState::UAV)) result |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	if (hasAnyFlags(state, ResourceState::VERTEX_BUFFER)) result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	if (hasAnyFlags(state, ResourceState::INDEX_BUFFER)) result |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
	if (hasAnyFlags(state, ResourceState::INDIRECT_ARGS)) result |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	return result;
}

DX12Buffer::DX12Buffer(BufferDescription description) : RHIBuffer(description)
{
	auto *native_rhi = DX12Utils::getNativeRHI();

	D3D12_RESOURCE_FLAGS resource_flags = D3D12_RESOURCE_FLAG_NONE;

	if (hasAnyFlags(description.usage, BufferUsage::SHADER_WRITE_BUFFER | BufferUsage::SCRATCH_BUFFER | BufferUsage::ACCELERATION_STRUCTURE_STORAGE_BUFFER))
		resource_flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(description.size, resource_flags);
	D3D12_RESOURCE_STATES resource_state = description.use_staging_buffer ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_GENERIC_READ;
	current_state = description.use_staging_buffer ? ResourceState::COMMON : ResourceState::COPY_SRC;

	if (hasAnyFlags(description.usage, BufferUsage::ACCELERATION_STRUCTURE_STORAGE_BUFFER))
		resource_state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	else if (hasAnyFlags(description.usage, BufferUsage::ACCELERATION_STRUCTURE_STORAGE_BUFFER))
		resource_state = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

	D3D12MA::ALLOCATION_DESC allocation_desc = {};
	if (hasAnyFlags(description.usage, BufferUsage::READBACK_BUFFER))
	{
		allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
		resource_state = D3D12_RESOURCE_STATE_COPY_DEST;
		current_state = ResourceState::COPY_DST;
	} else
	{
		allocation_desc.HeapType = description.use_staging_buffer && !hasAnyFlags(description.usage, BufferUsage::STAGING_BUFFER) ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD;
	}

	allocation = std::make_unique<DX12AllocationResource>();
	resource = std::make_unique<DX12Resource>();

	HRESULT res = native_rhi->allocator->CreateResource(
		&allocation_desc,
		&resource_desc,
		resource_state,
		nullptr,
		&allocation->resource,
		IID_PPV_ARGS(&resource->resource));
	ENGINE_ASSERT(SUCCEEDED(res));
}

DX12Buffer::~DX12Buffer()
{
	auto *native_rhi = DX12Utils::getNativeRHI();
	native_rhi->releaseGPUResource(resource.release());
	native_rhi->releaseGPUResource(allocation.release());
}

void DX12Buffer::fill(const void *sourceData)
{
	ENGINE_ASSERT(sourceData);
	PROFILE_CPU_FUNCTION();

	auto *native_rhi = DX12Utils::getNativeRHI();
	uint64_t buffer_size = description.size;

	if (description.use_staging_buffer && !hasAnyFlags(description.usage, BufferUsage::STAGING_BUFFER))
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
		current_state = ResourceState::SHADER_RESOURCE;

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
	ENGINE_ASSERT(data);
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

RHIBufferView *DX12Buffer::getShaderResourceView()
{
	if (!shader_resource_view)
		shader_resource_view = new DX12BufferView(BufferViewDescription(this, BufferViewType::SHADER_RESOURCE));
	return shader_resource_view;
}

RHIBufferView *DX12Buffer::getUnorderedAccessView(bool force_raw)
{
	if (force_raw)
	{
		if (!raw_unordered_access_view)
			raw_unordered_access_view = new DX12BufferView(BufferViewDescription(this, BufferViewType::SHADER_RESOURCE_STORAGE_RAW));
		return raw_unordered_access_view;
	}
	if (!unordered_access_view)
		unordered_access_view = new DX12BufferView(BufferViewDescription(this, BufferViewType::SHADER_RESOURCE_STORAGE));
	return unordered_access_view;
}

void DX12Buffer::transitState(ResourceState new_state)
{
	DX12CommandList *native_cmd_list = (DX12CommandList *)gDynamicRHI->getCmdList();

	if (current_state == new_state)
	{
		if (hasAnyFlags(new_state, ResourceState::UAV))
		{
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barrier.UAV.pResource = resource->resource;

			native_cmd_list->cmd_list->ResourceBarrier(1, &barrier);
		}
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource->resource;
	barrier.Transition.StateBefore = toDX12ResourceState(current_state);
	barrier.Transition.StateAfter = toDX12ResourceState(new_state);
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	native_cmd_list->cmd_list->ResourceBarrier(1, &barrier);

	current_state = new_state;
}

DX12BufferView::DX12BufferView(BufferViewDescription description) : RHIBufferView(description)
{
	ENGINE_ASSERT(description.buffer);

	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

	// Allocate descriptor in staging heap
	descriptor = rhi->cbv_srv_uav_staging_heap->allocate();

	DX12Buffer *native_buffer = static_cast<DX12Buffer *>(description.buffer);

	bool is_raw = native_buffer->getStride() == sizeof(uint32_t);
	ENGINE_ASSERT_MSG(native_buffer->getStride() > 0, "Cannot create buffer with zero stride");
	if (description.view_type == BufferViewType::SHADER_RESOURCE)
	{

		D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

		if (is_raw)
		{
			srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
			srv_desc.Buffer.FirstElement = 0;
			srv_desc.Buffer.NumElements = native_buffer->getSize() / sizeof(uint32_t);
			srv_desc.Buffer.StructureByteStride = 0;
			srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		} else
		{
			srv_desc.Format = DXGI_FORMAT_UNKNOWN;
			srv_desc.Buffer.FirstElement = 0;
			srv_desc.Buffer.NumElements = native_buffer->getSize() / native_buffer->getStride();
			srv_desc.Buffer.StructureByteStride = native_buffer->getStride();
			srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		}

		rhi->device->CreateShaderResourceView(native_buffer->getResource(), &srv_desc, descriptor.getCpuHandle());
		bindless_index = gDynamicRHI->getBindlessResources()->addBuffer(this);
	} else if (description.view_type == BufferViewType::SHADER_RESOURCE_STORAGE || description.view_type == BufferViewType::SHADER_RESOURCE_STORAGE_RAW)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
		uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		bool use_raw = is_raw || description.view_type == BufferViewType::SHADER_RESOURCE_STORAGE_RAW;
		if (use_raw)
		{
			uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
			uav_desc.Buffer.FirstElement = 0;
			uav_desc.Buffer.NumElements = native_buffer->getSize() / sizeof(uint32_t);
			uav_desc.Buffer.StructureByteStride = 0;
			uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		} else
		{
			uav_desc.Format = DXGI_FORMAT_UNKNOWN;
			uav_desc.Buffer.FirstElement = 0;
			uav_desc.Buffer.NumElements = native_buffer->getSize() / native_buffer->getStride();
			uav_desc.Buffer.StructureByteStride = native_buffer->getStride();
			uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		}

		rhi->device->CreateUnorderedAccessView(native_buffer->getResource(), nullptr, &uav_desc, descriptor.getCpuHandle());
		bindless_index = gDynamicRHI->getBindlessResources()->addBuffer(this);
	}
}

DX12BufferView::~DX12BufferView()
{
	if (gDynamicRHI && gDynamicRHI->getBindlessResources())
		gDynamicRHI->getBindlessResources()->removeBuffer(this);
}
