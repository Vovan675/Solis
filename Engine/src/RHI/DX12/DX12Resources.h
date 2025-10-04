#pragma once
#include "RHI/RHIDefinitions.h"
#include <d3d12.h>
#include "D3D12MemoryAllocator/D3D12MemAlloc.h"

struct DX12PipelineResource final: public RenderResource
{
	void Release() override;

	ID3D12PipelineState *pipeline_state;
	ID3D12RootSignature *root_signature;

	ID3D12StateObject *rt_pso;
	ID3D12StateObjectProperties *rt_props;
};

struct DX12Resource final: public RenderResource
{
	void Release() override;

	ID3D12Resource *resource;
};

struct DX12AllocationResource final: public RenderResource
{
	void Release() override;

	D3D12MA::Allocation *resource;
};