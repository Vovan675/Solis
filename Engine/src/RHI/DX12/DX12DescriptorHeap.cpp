#include "pch.h"
#include "DX12DescriptorHeap.h"
#include "DX12Utils.h"

DX12DescriptorHeap::DX12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptors_count, bool shader_visible)
{
	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
	heap_desc.Type = type;
	heap_desc.NumDescriptors = descriptors_count;
	heap_desc.Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	auto *native_rhi = DX12Utils::getNativeRHI();
	native_rhi->device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap));

	stride = native_rhi->device->GetDescriptorHandleIncrementSize(type);

	start_descriptor.cpu_handle = heap->GetCPUDescriptorHandleForHeapStart();
	if (shader_visible)
		start_descriptor.gpu_handle = heap->GetGPUDescriptorHandleForHeapStart();

	this->descriptors_count = descriptors_count;
}

DX12FrameDescriptorHeap::DX12FrameDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t descriptors_count, uint32_t reserved_start)
{
	assert(descriptors_count > reserved_start);
	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
	heap_desc.Type = type;
	heap_desc.NumDescriptors = descriptors_count;
	heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	auto *native_rhi = DX12Utils::getNativeRHI();
	native_rhi->device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heap));

	stride = native_rhi->device->GetDescriptorHandleIncrementSize(type);
	start_descriptor.cpu_handle = heap->GetCPUDescriptorHandleForHeapStart();
	start_descriptor.gpu_handle = heap->GetGPUDescriptorHandleForHeapStart();

	this->reserved_start = reserved_start;
	max_size = descriptors_count - reserved_start;
}
