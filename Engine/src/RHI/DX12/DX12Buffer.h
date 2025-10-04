#pragma once
#include "RHI/DynamicRHI.h"
#include "RHI/RHIBuffer.h"
#include "DX12Resources.h"
#include "D3D12MemoryAllocator/D3D12MemAlloc.h"

enum ResourceState
{
	RESOURCE_STATE_UNDEFINED = 0,
	RESOURCE_STATE_COMMON = 1 << 0,
	RESOURCE_STATE_RENDER_TARGET = 1 << 1,
	RESOURCE_STATE_SHADER_RESOURCE = 1 << 2,
	RESOURCE_STATE_COPY_SRC = 1 << 3,
	RESOURCE_STATE_COPY_DST = 1 << 4,
	RESOURCE_STATE_UAV = 1 << 5,
	RESOURCE_STATE_PRESENT = 1 << 6,

	RESOURCE_STATE_VERTEX_BUFFER = 1 << 7,
	RESOURCE_STATE_INDEX_BUFFER = 1 << 8,
};

static D3D12_RESOURCE_STATES toDX12ResourceState(ResourceState state)
{
	D3D12_RESOURCE_STATES result = D3D12_RESOURCE_STATE_COMMON; // = 0
	if (state & RESOURCE_STATE_SHADER_RESOURCE) result |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	if (state & RESOURCE_STATE_COPY_SRC) result |= D3D12_RESOURCE_STATE_COPY_SOURCE;
	if (state & RESOURCE_STATE_COPY_DST) result |= D3D12_RESOURCE_STATE_COPY_DEST;
	if (state & RESOURCE_STATE_UAV) result |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	if (state & RESOURCE_STATE_VERTEX_BUFFER) result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	if (state & RESOURCE_STATE_INDEX_BUFFER) result |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
	return result;
}

class DX12DynamicRHI;
class DX12Buffer final: public RHIBuffer
{
public:
	DX12Buffer(BufferDescription description);
	~DX12Buffer();

	void destroy();

	void fill(const void *sourceData) override;
	void map(void **data) override;
	void unmap() override;

	void setDebugName(const char *name) override;

	uint64_t getGPUAddress() const override
	{
		return resource->resource->GetGPUVirtualAddress();
	}

	D3D12_VERTEX_BUFFER_VIEW getVertexBufferView() const
	{
		D3D12_VERTEX_BUFFER_VIEW view;
		view.BufferLocation = resource->resource->GetGPUVirtualAddress();
		view.SizeInBytes = description.size;
		view.StrideInBytes = description.vertex_buffer_stride;
		return view;
	}

	D3D12_INDEX_BUFFER_VIEW getIndexBufferView() const
	{
		D3D12_INDEX_BUFFER_VIEW view;
		view.BufferLocation = resource->resource->GetGPUVirtualAddress();
		view.SizeInBytes = description.size;
		view.Format = DXGI_FORMAT_R32_UINT; //16?
		return view;
	}

	void setState(ResourceState new_state);

	std::unique_ptr<DX12AllocationResource> allocation;
	std::unique_ptr<DX12Resource> resource;
	bool is_mapped = false;

	ResourceState current_state;
};