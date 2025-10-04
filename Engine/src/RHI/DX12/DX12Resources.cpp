#include "pch.h"
#include "DX12Resources.h"
#include "DX12Utils.h"

void DX12PipelineResource::Release()
{
	SAFE_RELEASE(pipeline_state);
	SAFE_RELEASE(root_signature);
	SAFE_RELEASE(rt_pso);
	SAFE_RELEASE(rt_props);
}

void DX12Resource::Release()
{
	resource->Release();
}

void DX12AllocationResource::Release()
{
	resource->Release();
}
