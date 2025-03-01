#pragma once
#include "RHI/RHIPipeline.h"

class DX12Pipeline : public RHIPipeline
{
public:
	DX12Pipeline(ID3D12Device *device): device(device) {}
	~DX12Pipeline();

	void destroy() override;

	void create(const PipelineDescription &description) override;

	PipelineDescription description;

	ID3D12Device *device;
	ComPtr<ID3D12PipelineState> pipeline;
	ComPtr<ID3D12RootSignature> root_signature;

	DX12Shader::BindingInfo binding_info;
};