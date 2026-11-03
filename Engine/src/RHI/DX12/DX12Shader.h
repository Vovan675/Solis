#pragma once
#include "RHI/RHIShader.h"

class DX12Shader final: public RHIShader {
public:
	DX12Shader(const eastl::wstring &path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines);

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

		eastl::vector<ConstantBufferInfo> constant_buffers;
		eastl::vector<ConstantBufferInfo> acceleration_structures;

		int srv_bindless;
		int samplers_bindless;
	};
	static eastl::vector<CD3DX12_ROOT_PARAMETER1> getRootParameters(const eastl::fixed_vector<RHIShader *, 3> &shaders, BindingInfo &binding_info);

	ComPtr<IDxcBlob> blob; // Compiled shader bytecode

	ComPtr<ID3D12ShaderReflection> reflection;
	ComPtr<ID3D12LibraryReflection> reflection_library;

private:
	void load_bytecode(IDxcBlob *bytecode) override;
};
