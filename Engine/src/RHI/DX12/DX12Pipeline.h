#pragma once
#include "RHI/RHIPipeline.h"
#include "DX12Resources.h"
#include "DX12Shader.h"

class DX12Pipeline final: public RHIPipeline
{
public:
	DX12Pipeline(ID3D12Device *device, const PipelineDescription &description);
	~DX12Pipeline();

	ID3D12Device *device;
	std::unique_ptr<DX12PipelineResource> pipeline;

	DX12Shader::BindingInfo binding_info;

	void fillDispatchRaysDesc(D3D12_DISPATCH_RAYS_DESC &desc);
	RHIBufferRef sbt_buffer;
};