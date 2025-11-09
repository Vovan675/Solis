#pragma once
#include "RHI/DynamicRHI.h"
#include "RHI/RHIBuffer.h"
#include "DX12Resources.h"
#include "DX12DescriptorHeap.h"
#include "D3D12MemoryAllocator/D3D12MemAlloc.h"

class DX12BufferView;

class DX12Buffer final: public RHIBuffer
{
public:
	DX12Buffer(BufferDescription description);
	~DX12Buffer();

	void fill(const void *sourceData) override;
	void map(void **data) override;
	void unmap() override;

	void setDebugName(const char *name) override;

	uint64_t getGPUAddress() const override
	{
		return resource->resource->GetGPUVirtualAddress();
	}

	RHIBufferView *getShaderResourceView() override;
	RHIBufferView *getUnorderedAccessView() override;

public:
	ID3D12Resource *getResource() const { return resource->resource; }
	D3D12MA::Allocation *getAllocation() const { return allocation->resource; }

	void setState(ResourceState new_state);
private:
	std::unique_ptr<DX12Resource> resource;
	std::unique_ptr<DX12AllocationResource> allocation;
	
	ResourceState current_state;
	Ref<DX12BufferView> shader_resource_view;
	Ref<DX12BufferView> unordered_access_view;

	bool is_mapped = false;
};

class DX12BufferView final: public RHIBufferView
{
public:
	DX12BufferView(BufferViewDescription description);
	~DX12BufferView();

	const DX12Descriptor &getDescriptor() const { return descriptor; }

private:
	DX12Descriptor descriptor;
};