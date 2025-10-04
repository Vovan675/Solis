#pragma once
#include "RHI/RHIPipeline.h"
#include "DX12Resources.h"

class DX12Pipeline final: public RHIPipeline
{
public:
	DX12Pipeline(ID3D12Device *device): device(device) {}
	~DX12Pipeline();

	void destroy();

	void create(const PipelineDescription &description) override;

	PipelineDescription description;

	ID3D12Device *device;
	std::unique_ptr<DX12PipelineResource> pipeline;

	DX12Shader::BindingInfo binding_info;

	void fillDispatchRaysDesc(D3D12_DISPATCH_RAYS_DESC &desc);
	RHIBufferRef sbt_buffer;
};