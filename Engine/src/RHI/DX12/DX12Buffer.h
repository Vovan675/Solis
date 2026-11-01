#pragma once
#include "RHI/DynamicRHI.h"
#include "RHI/RHIBuffer.h"
#include "DX12Resources.h"
#include "DX12DescriptorHeap.h"
#include "D3D12MemoryAllocator/D3D12MemAlloc.h"

class DX12Buffer final: public RHIBuffer
{
public:
	DX12Buffer(BufferDescription description);
	~DX12Buffer();

	void map(void **data) override;
	void unmap() override;

	void setDebugName(const char *name) override;

	uint64_t getGPUAddress() const override
	{
		return resource->resource->GetGPUVirtualAddress();
	}

	void transitState(RHICommandList *cmd_list, ResourceState new_state) override;

	bool isValid() const override { return resource != nullptr; }

public:
	ID3D12Resource *getResource() const { return resource->resource; }
	D3D12MA::Allocation *getAllocation() const { return allocation->resource; }

private:
	RHIBufferView *create_view(const BufferViewDescription &view_description) override;

	std::unique_ptr<DX12Resource> resource;
	std::unique_ptr<DX12AllocationResource> allocation;

	ResourceState current_state;
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
