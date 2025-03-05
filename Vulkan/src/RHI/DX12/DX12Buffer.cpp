#include "pch.h"
#include "DX12Buffer.h"
#include "DX12DynamicRHI.h"
#include "DX12Utils.h"

DX12Buffer::DX12Buffer(BufferDescription description) : RHIBuffer(description)
{
	auto *native_rhi = DX12Utils::getNativeRHI();

	D3D12_HEAP_TYPE heap_type = description.useStagingBuffer ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD;
	native_rhi->device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(heap_type), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(description.size, D3D12_RESOURCE_FLAG_NONE),
									description.useStagingBuffer ? D3D12_RESOURCE_STATE_COMMON
									: D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
}

DX12Buffer::~DX12Buffer()
{
	destroy();
}

void DX12Buffer::destroy()
{
	auto *native_rhi = DX12Utils::getNativeRHI();
	if (resource)
	{
		native_rhi->gpu_release_queue[native_rhi->current_frame].push_back({RESOURCE_TYPE_BUFFER, resource.Detach()});
	}
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
						   resource.Get(), intermediate_resource.Get(),
						   0, 0, 1, &subresourceData);

		copy_cmd_list->close();
		native_rhi->getCmdQueueCopy()->execute(copy_cmd_list);


		// Wait queue
		auto last_fence = native_rhi->getCmdQueueCopy()->getLastFenceValue();
		native_rhi->getCmdQueueCopy()->signal(last_fence + 1);
		native_rhi->getCmdQueueCopy()->wait(last_fence + 1);

		intermediate_resource.Reset();
	} else
	{
		// For CPU-visible (upload) buffers, map and write directly.
		void* data = nullptr;
		CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
		resource->Map(0, &readRange, &data);
		memcpy(data, sourceData, buffer_size);
		resource->Unmap(0, nullptr);
	}
}

void DX12Buffer::map(void **data)
{
	CD3DX12_RANGE read_range(0, 0);
	resource->Map(0, &read_range, data);
}

void DX12Buffer::unmap()
{
	resource->Unmap(0, nullptr);
}

void DX12Buffer::setDebugName(const char *name)
{
	wchar_t wbuf[1024];
	size_t l = std::min(size_t(1023), strlen(name));
	wbuf[l] = '\0';
	mbstowcs(wbuf, name, l);

	resource->SetName(wbuf);
}
