#include "pch.h"
#include "DX12CommandQueue.h"
#include "DX12DynamicRHI.h"

DX12CommandQueue::~DX12CommandQueue()
{
	cmd_queue.Reset();
	SAFE_RELEASE(fence);
	CloseHandle(fence_event);
}

uint64_t DX12CommandQueue::execute(eastl::span<RHICommandList *> cmd_lists)
{
	eastl::vector<ID3D12CommandList *> native_cmd_lists;
	native_cmd_lists.reserve(cmd_lists.size());
	for (RHICommandList *cmd_list : cmd_lists)
		native_cmd_lists.push_back(static_cast<DX12CommandList *>(cmd_list)->cmd_list.Get());
	cmd_queue->ExecuteCommandLists(native_cmd_lists.size(), native_cmd_lists.data());

	// Set fence value on execution end
	last_fence_value++;
	cmd_queue->Signal(fence, last_fence_value);
	return last_fence_value;
}

void DX12CommandQueue::waitForFence(uint64_t fence_value)
{
	PROFILE_CPU_FUNCTION();
	// Wait for fence to complete
	if (fence->GetCompletedValue() < fence_value)
	{
		fence->SetEventOnCompletion(fence_value, fence_event);
		WaitForSingleObjectEx(fence_event, INFINITE, false);
	}
}

void DX12CommandQueue::waitIdle()
{
	last_fence_value++;
	cmd_queue->Signal(fence, last_fence_value);
	waitForFence(last_fence_value);
}
