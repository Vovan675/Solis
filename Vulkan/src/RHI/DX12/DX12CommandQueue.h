#pragma once
#include "RHI/RHICommandQueue.h"

class DX12CommandQueue: public RHICommandQueue
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

	void execute(RHICommandList *cmd_list) override;

	void signal(uint64_t fence_value) override
	{
		// Set fence value on execution end
		cmd_queue->Signal(fence, fence_value);
		last_fence_value = fence_value;
	}

	void wait(uint64_t fence_value) override
	{
		// Wait for fence to complete
		if (fence->GetCompletedValue() < fence_value)
		{
			fence->SetEventOnCompletion(fence_value, fence_event);
			WaitForSingleObjectEx(fence_event, INFINITE, false);
		}
	}

	void waitIdle() override
	{
	}

	uint32_t getLastFenceValue() override
	{
		return last_fence_value;
	}

	ComPtr<ID3D12CommandQueue> cmd_queue;
	ID3D12Fence *fence;
	HANDLE fence_event;

	uint32_t last_fence_value = 0;
};