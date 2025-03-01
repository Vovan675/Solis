#pragma once
#include "RHI/DynamicRHI.h"
#include "RHI/RHIBuffer.h"

class DX12DynamicRHI;
class DX12Buffer : public RHIBuffer
{
public:
	DX12Buffer(DX12DynamicRHI *rhi, ComPtr<ID3D12Device> device, BufferDescription description);
	~DX12Buffer();

	void destroy() override;

	void fill(const void *sourceData) override;
	void map(void **data) override;
	void unmap() override;

	void setDebugName(const char *name) override;

	D3D12_VERTEX_BUFFER_VIEW getVertexBufferView() const
	{
		D3D12_VERTEX_BUFFER_VIEW view;
		view.BufferLocation = resource->GetGPUVirtualAddress();
		view.SizeInBytes = description.size;
		view.StrideInBytes = description.vertex_buffer_stride;
		return view;
	}

	D3D12_INDEX_BUFFER_VIEW getIndexBufferView() const
	{
		D3D12_INDEX_BUFFER_VIEW view;
		view.BufferLocation = resource->GetGPUVirtualAddress();
		view.SizeInBytes = description.size;
		view.Format = DXGI_FORMAT_R32_UINT; //16?
		return view;
	}

	DX12DynamicRHI *rhi;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12Resource> resource;

	ComPtr<ID3D12Resource> intermediate_resource;
	bool is_mapped = false;
};