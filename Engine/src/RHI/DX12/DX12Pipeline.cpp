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
	native_rhi->releaseGPUResource(pipeline.release());
	sbt_buffer = nullptr;
}

void DX12Pipeline::create(const PipelineDescription &description)
{
	PROFILE_CPU_FUNCTION();
	destroy();

	pipeline = std::make_unique<DX12PipelineResource>();

	this->description = description;
	hash = description.getHash();

	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | // For ResourceDescriptorHeap access
		D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED | // For SamplerDescriptorHeap access
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;// |
	//D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	// Root signature should come from shader reflection

	std::vector<DX12Shader *> shaders;

	if (description.is_compute_pipeline)
	{
		shaders.push_back(static_cast<DX12Shader *>(description.compute_shader.getReference()));
	} else if (description.is_ray_tracing_pipeline)
	{
		shaders.push_back(static_cast<DX12Shader *>(description.ray_generation_shader.getReference()));
		shaders.push_back(static_cast<DX12Shader *>(description.miss_shader.getReference()));
		shaders.push_back(static_cast<DX12Shader *>(description.closest_hit_shader.getReference()));
	} else
	{
		shaders.push_back(static_cast<DX12Shader *>(description.vertex_shader.getReference()));
		shaders.push_back(static_cast<DX12Shader *>(description.fragment_shader.getReference()));
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
	device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pipeline->root_signature));

	if (description.is_compute_pipeline)
	{
		DX12Shader *cs = static_cast<DX12Shader *>(description.compute_shader.getReference());

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = pipeline->root_signature;
		psoDesc.CS = {cs->m_blob->GetBufferPointer(), cs->m_blob->GetBufferSize()};
		psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipeline->pipeline_state));
	} else if (description.is_ray_tracing_pipeline)
	{
		DX12Shader *ray_gen = static_cast<DX12Shader *>(description.ray_generation_shader.getReference());
		DX12Shader *miss = static_cast<DX12Shader *>(description.miss_shader.getReference());
		DX12Shader *closest_hit = static_cast<DX12Shader *>(description.closest_hit_shader.getReference());


		CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

		auto addLibrary = [](CD3DX12_STATE_OBJECT_DESC& raytracingPipeline, IDxcBlob *s, const std::vector<const wchar_t*>& export_)
		{
			auto raygen_lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
			D3D12_SHADER_BYTECODE shaderBytecode;
			shaderBytecode.pShaderBytecode = s->GetBufferPointer();
			shaderBytecode.BytecodeLength = s->GetBufferSize();
			raygen_lib->SetDXILLibrary(&shaderBytecode);

			for (auto e : export_)
				raygen_lib->DefineExport(e);
		};

		const wchar_t* raygenname = L"RayGen";
		const wchar_t* missname = L"Miss";
		const wchar_t* hitname = L"ClosestHit";
		const wchar_t* hitGroupName = L"HitGroup";

		addLibrary(raytracingPipeline, ray_gen->m_blob.Get(), {raygenname});
		addLibrary(raytracingPipeline, miss->m_blob.Get(), { missname });
		addLibrary(raytracingPipeline, closest_hit->m_blob.Get(), { hitname });

		// Triangle hit group
		auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
		hitGroup->SetClosestHitShaderImport(hitname);
		hitGroup->SetHitGroupExport(hitGroupName);
		hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);


		// Shader config
		// Defines the maximum sizes in bytes for the ray payload and attribute structure.
		auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
		UINT payloadSize = 4 * sizeof(float);   // float4 color
		UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
		shaderConfig->Config(payloadSize, attributeSize);

		// Local root signature and shader association
		///CreateRaygenLocalSignatureSubobject(&raytracingPipeline, hitGroupName, raytracingLocalRootSignature.Get());
		// This is a root signature that enables a shader to have unique arguments that come from shader tables.

		// Global root signature
		// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
		auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
		globalRootSignature->SetRootSignature(pipeline->root_signature);


		// Pipeline config
		// Defines the maximum TraceRay() recursion depth.
		auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
		// PERFOMANCE TIP: Set max recursion depth as low as needed 
		// as drivers may apply optimization strategies for low recursion depths. 
		UINT maxRecursionDepth = 1; // ~ primary rays only + TraceRayInline(). 
		pipelineConfig->Config(maxRecursionDepth);

		DX12Utils::getNativeRHI()->device->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&pipeline->rt_pso));

		pipeline->rt_pso->QueryInterface(IID_PPV_ARGS(&pipeline->rt_props));
	} else
	{
		DX12Shader *vs = static_cast<DX12Shader *>(description.vertex_shader.getReference());
		DX12Shader *ps = static_cast<DX12Shader *>(description.fragment_shader.getReference());


		std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout;

		uint32_t offset = 0;
		for (auto &input : description.vertex_inputs_descriptions.inputs)
		{
			input_layout.push_back({input.semantic_name, input.semantic_index, DX12Utils::getNativeFormat(input.format), 0, offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
			offset += Math::alignedSize(getFormatSize(input.format), 16);
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
		psoDesc.pRootSignature = pipeline->root_signature;
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
		device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline->pipeline_state));
	}
}

void DX12Pipeline::fillDispatchRaysDesc(D3D12_DISPATCH_RAYS_DESC &desc)
{
	BufferDescription sbt_buffer_desc;
	sbt_buffer_desc.size = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT * 3;
	sbt_buffer_desc.useStagingBuffer = false;
	sbt_buffer_desc.alignment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
	if (!sbt_buffer)
	{
		sbt_buffer = gDynamicRHI->createBuffer(sbt_buffer_desc);

		void *data;
		sbt_buffer->map(&data);

		void *raygen_id = pipeline->rt_props->GetShaderIdentifier(L"RayGen");
		void *miss_id = pipeline->rt_props->GetShaderIdentifier(L"Miss");
		void *hitgroup_id = pipeline->rt_props->GetShaderIdentifier(L"HitGroup");

		uint8_t *current = (uint8_t *)data;

		// Raygen
		memcpy(current, raygen_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		current += D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

		// Miss
		memcpy(current, miss_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		current += D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

		// Closest Hit
		memcpy(current, hitgroup_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		current += D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

		sbt_buffer->unmap();
	}

	uint64_t start_address = sbt_buffer->getGPUAddress();

	desc.RayGenerationShaderRecord.StartAddress = start_address;
	desc.RayGenerationShaderRecord.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

	desc.MissShaderTable.StartAddress = start_address + D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
	desc.MissShaderTable.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	desc.MissShaderTable.StrideInBytes = desc.MissShaderTable.StrideInBytes;

	desc.HitGroupTable.StartAddress = start_address + D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT * 2;
	desc.HitGroupTable.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	desc.HitGroupTable.StrideInBytes = desc.HitGroupTable.SizeInBytes;
}
