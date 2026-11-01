#pragma once
#include "RHI/RHICommandQueue.h"

class DX12CommandQueue final: public RHICommandQueue
{
public:
	DX12CommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type)
	{
		D3D12_COMMAND_QUEUE_DESC queue_desc_copy{};
		queue_desc_copy.Type = type;
		device->CreateCommandQueue(&queue_desc_copy, IID_PPV_ARGS(&cmd_queue));

		device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		fence->SetName(L"Command Queue Fence");
		fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	}

	~DX12CommandQueue();

	uint64_t execute(eastl::span<RHICommandList *> cmd_lists) override;
	void waitForFence(uint64_t fence_value) override;
	void waitIdle() override;

	ComPtr<ID3D12CommandQueue> cmd_queue;
	ID3D12Fence *fence;
	HANDLE fence_event;
};