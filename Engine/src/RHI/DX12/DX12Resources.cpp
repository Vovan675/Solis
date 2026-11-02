#include "pch.h"
#include "DX12Resources.h"
#include "DX12Utils.h"

void DX12PipelineResource::release()
{
	SAFE_RELEASE(pipeline_state);
	SAFE_RELEASE(root_signature);
	SAFE_RELEASE(rt_pso);
	SAFE_RELEASE(rt_props);
}

void DX12Resource::release()
{
	resource->Release();
}

void DX12AllocationResource::release()
{
	resource->Release();
}
