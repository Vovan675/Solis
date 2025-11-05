#pragma once
#include "RHI/RHIShader.h"

class DX12Shader final: public RHIShader {
public:
	DX12Shader(const eastl::wstring &path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines, IDxcUtils* dxc_utils);
	~DX12Shader() { destroy(); }

	void destroy();
	void recompile() override;

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
	static eastl::vector<CD3DX12_ROOT_PARAMETER1> getRootParameters(eastl::vector<DX12Shader *> shaders, BindingInfo &binding_info);

	ComPtr<IDxcBlob> blob; // Compiled shader bytecode

	IDxcUtils *dxc_utils;

	ComPtr<ID3D12ShaderReflection> reflection;
	ComPtr<ID3D12LibraryReflection> reflection_library;
	eastl::hash_set<eastl::wstring> included_files;
};