#pragma once
#include "RHI/RHIShader.h"

class DX12Shader : public RHIShader {
public:
	DX12Shader(ComPtr<IDxcBlob> blob, ComPtr<ID3D12ShaderReflection> reflection, ShaderType type, size_t hash) 
		: m_blob(blob), reflection(reflection), type(type)
	{
		this->hash = hash;
	}

	struct TableInfo
	{
		int table_index = -1;
		int begin_register = -1;
		int end_register = -1;
		int registersCount() const { return end_register - begin_register; }
	};

	struct ConstantBufferInfo
	{
		int root_param_index;
		int bind_point;
	};
	struct BindingInfo
	{
		TableInfo srv_table;
		TableInfo samplers_table;
		TableInfo uav_table;

		std::vector<ConstantBufferInfo> constant_buffers;

		int srv_bindless;
		int samplers_bindless;
	};
	static std::vector<CD3DX12_ROOT_PARAMETER1> getRootParameters(std::vector<std::shared_ptr<DX12Shader>> shaders, BindingInfo &binding_info);

	ShaderType type;
	ComPtr<IDxcBlob> m_blob; // Compiled shader bytecode

	ComPtr<ID3D12ShaderReflection> reflection;
};