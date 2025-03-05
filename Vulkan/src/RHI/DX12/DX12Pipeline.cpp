#include "pch.h"
#include "DX12Pipeline.h"
#include "DX12DynamicRHI.h"
#include "DX12Utils.h"
#include "Utils/Math.h"

DX12Pipeline::~DX12Pipeline()
{
	destroy();
}

void DX12Pipeline::destroy()
{
	auto *native_rhi = DX12Utils::getNativeRHI();
	if (pipeline)
	{
		native_rhi->gpu_release_queue[native_rhi->current_frame].push_back({RESOURCE_TYPE_PIPELINE, pipeline.Detach()});
		native_rhi->gpu_release_queue[native_rhi->current_frame].push_back({RESOURCE_TYPE_ROOT_SIGNATURE, root_signature.Detach()});
	}
}

void DX12Pipeline::create(const PipelineDescription &description)
{
	PROFILE_CPU_FUNCTION();
	destroy();
	this->description = description;
	hash = description.getHash();

	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;// |
	//D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	// Root signature should come from shader reflection

	std::vector<std::shared_ptr<DX12Shader>> shaders;

	if (description.is_compute_pipeline)
	{
		shaders.push_back(std::static_pointer_cast<DX12Shader>(description.compute_shader));
	} else
	{
		shaders.push_back(std::static_pointer_cast<DX12Shader>(description.vertex_shader));
		shaders.push_back(std::static_pointer_cast<DX12Shader>(description.fragment_shader));
	}

	auto root_params = DX12Shader::getRootParameters(shaders, binding_info);

	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 1;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(root_params.size(), root_params.data(), 0, nullptr, rootSignatureFlags);


	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	// Serialize the root signature.
	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	HRESULT res = D3DX12SerializeVersionedRootSignature(&rootSignatureDescription, featureData.HighestVersion, &rootSignatureBlob, &errorBlob);
	// Create the root signature.
	device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&root_signature));

	if (description.is_compute_pipeline)
	{
		DX12Shader *cs = static_cast<DX12Shader *>(description.compute_shader.get());

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = root_signature.Get();
		psoDesc.CS = {cs->m_blob->GetBufferPointer(), cs->m_blob->GetBufferSize()};
		psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipeline));
	} else
	{
		DX12Shader *vs = static_cast<DX12Shader *>(description.vertex_shader.get());
		DX12Shader *ps = static_cast<DX12Shader *>(description.fragment_shader.get());


		std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout;

		uint32_t offset = 0;
		for (auto &input : description.vertex_inputs_descriptions.inputs)
		{
			input_layout.push_back({input.semantic_name, input.semantic_index, DX12Utils::getNativeFormat(input.format), 0, offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
			offset += Math::alignedSize(DX12Utils::getFormatSize(input.format), 16);
		}

		D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
		switch (description.primitive_topology)
		{
			case TOPOLOGY_POINT_LIST: topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; break;
			case TOPOLOGY_LINE_LIST: topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; break;
			case TOPOLOGY_TRIANGLE_LIST: topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
			case TOPOLOGY_TRIANGLE_STRIP: topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
		}

		D3D12_CULL_MODE cull_mode = D3D12_CULL_MODE_BACK;
		switch (description.cull_mode)
		{
			case CULL_MODE_NONE: cull_mode = D3D12_CULL_MODE_NONE; break;
			case CULL_MODE_BACK: cull_mode = D3D12_CULL_MODE_BACK; break;
			case CULL_MODE_FRONT: cull_mode = D3D12_CULL_MODE_FRONT; break;
		}

		D3D12_DEPTH_STENCIL_DESC depth_stencil_desc = {};
		depth_stencil_desc.DepthEnable = description.use_depth_test;
		depth_stencil_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		depth_stencil_desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		depth_stencil_desc.StencilEnable = false;
		depth_stencil_desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		depth_stencil_desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
		const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS};
		depth_stencil_desc.FrontFace = defaultStencilOp;
		depth_stencil_desc.BackFace = defaultStencilOp;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = {input_layout.data(), (uint32_t)input_layout.size()};
		psoDesc.pRootSignature = root_signature.Get();
		psoDesc.VS = {vs->m_blob->GetBufferPointer(), vs->m_blob->GetBufferSize()};
		psoDesc.PS = {ps->m_blob->GetBufferPointer(), ps->m_blob->GetBufferSize()};
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		//psoDesc.RasterizerState.FrontCounterClockwise = true;
		psoDesc.RasterizerState.CullMode = cull_mode;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = depth_stencil_desc;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = topology;

		psoDesc.NumRenderTargets = description.color_formats.size();
		for (int i = 0; i < description.color_formats.size(); i++)
		{
			psoDesc.RTVFormats[i] = DX12Utils::getNativeFormat(description.color_formats[i]);
		}

		if (description.depth_format != FORMAT_UNDEFINED)
			psoDesc.DSVFormat = DX12Utils::getNativeFormat(description.depth_format);

		psoDesc.SampleDesc.Count = 1;
		device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline));
	}
}
