#include "pch.h"
#include "DX12CommandQueue.h"
#include "DX12DynamicRHI.h"

DX12CommandQueue::~DX12CommandQueue()
{
	cmd_queue.Reset();
	SAFE_RELEASE(fence);
	CloseHandle(fence_event);
}

void DX12CommandQueue::execute(RHICommandList *cmd_list)
{
	DX12CommandList *native_cmd_list = static_cast<DX12CommandList *>(cmd_list);
	ID3D12CommandList *ppCommandLists[] = {native_cmd_list->cmd_list.Get()};
	cmd_queue->ExecuteCommandLists(1, ppCommandLists);
}
