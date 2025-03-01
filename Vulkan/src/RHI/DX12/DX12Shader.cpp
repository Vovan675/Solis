#include "pch.h"
#include "DX12Shader.h"

static std::vector<CD3DX12_DESCRIPTOR_RANGE1> descriptor_ranges;
std::vector<CD3DX12_ROOT_PARAMETER1> DX12Shader::getRootParameters(std::vector<std::shared_ptr<DX12Shader>> shaders, BindingInfo &binding_info)
{
	std::vector<CD3DX12_ROOT_PARAMETER1> params;

	// Idea is to:
	// 1. Collect all info from shaders and skip identical
	// 2. Create root parameters from resulted info


	// 1. Collect data from all shaders
	struct Bindings
	{
		std::map<int, D3D12_SHADER_INPUT_BIND_DESC> binding_by_bind;
		bool alreadyDefined(D3D12_SHADER_INPUT_BIND_DESC &desc)
		{
			return binding_by_bind.find(desc.BindPoint) != binding_by_bind.end();
		}

		void set(D3D12_SHADER_INPUT_BIND_DESC &desc)
		{
			if (!alreadyDefined(desc))
				binding_by_bind[desc.BindPoint] = desc;
		}
	};

	binding_info = {};

	std::map<int, std::vector<D3D12_SHADER_INPUT_BIND_DESC>> cbv_by_space;

	std::map<int, std::vector<D3D12_SHADER_INPUT_BIND_DESC>> srv_by_space;
	std::map<int, std::vector<D3D12_SHADER_INPUT_BIND_DESC>> uav_by_space;
	std::map<int, std::vector<D3D12_SHADER_INPUT_BIND_DESC>> sampler_by_space;

	for (auto &shader : shaders)
	{
		D3D12_SHADER_VISIBILITY visibility;
		switch (shader->type)
		{
			case VERTEX_SHADER:
				visibility = D3D12_SHADER_VISIBILITY_VERTEX;
				break;
			case FRAGMENT_SHADER:
				visibility = D3D12_SHADER_VISIBILITY_PIXEL;
				break;
			case COMPUTE_SHADER:
				visibility = D3D12_SHADER_VISIBILITY_ALL;
				break;
		}

		auto isDefined = [](const std::vector<D3D12_SHADER_INPUT_BIND_DESC> &bindings, int bind_point) -> bool
		{
			for (const auto &binding : bindings)
			{
				if (binding.BindPoint == bind_point)
					return true;
			}
			return false;
		};


		D3D12_SHADER_DESC shader_desc;
		HRESULT res = shader->reflection->GetDesc(&shader_desc);
		for (int i = 0; i < shader_desc.BoundResources; i++)
		{
			D3D12_SHADER_INPUT_BIND_DESC bind_desc;
			shader->reflection->GetResourceBindingDesc(i, &bind_desc);

			if (bind_desc.Type == D3D_SIT_CBUFFER)
			{
				//const auto cb = shader->reflection->GetConstantBufferByIndex(bind_desc.BindPoint);
				//D3D12_SHADER_BUFFER_DESC cb_desc;
				//cb->GetDesc(&cb_desc);

				//auto &param = params.emplace_back();
				//param.InitAsConstants(cb_desc.Size / 4, bind_desc.BindPoint, bind_desc.Space, visibility);
				//param.InitAsConstantBufferView(bind_desc.BindPoint, bind_desc.Space, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, visibility);
				auto &bindings = cbv_by_space[bind_desc.Space];
				if (!isDefined(bindings, bind_desc.BindPoint))
				{
					bindings.push_back(bind_desc);

					ConstantBufferInfo &bind_info = binding_info.constant_buffers.emplace_back();
					bind_info.root_param_index = params.size();
					bind_info.bind_point = bind_desc.BindPoint;

					auto &param = params.emplace_back();
					//param.InitAsConstants(cb_desc.Size / 4, bind_desc.BindPoint, bind_desc.Space, visibility);
					param.InitAsConstantBufferView(bind_desc.BindPoint, bind_desc.Space, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
				}

				// also maybe pass params? can be used for setParameterByName in shader
			} else if (bind_desc.Type == D3D_SIT_TEXTURE)
			{
				auto &bindings = srv_by_space[bind_desc.Space];
				if (!isDefined(bindings, bind_desc.BindPoint))
				{
					bindings.push_back(bind_desc);
				}
			} else if (bind_desc.Type == D3D_SIT_SAMPLER)
			{
				auto &bindings = sampler_by_space[bind_desc.Space];
				if (!isDefined(bindings, bind_desc.BindPoint))
				{
					bindings.push_back(bind_desc);
				}
			} else if (bind_desc.Type == D3D_SIT_STRUCTURED)
			{
				auto &bindings = srv_by_space[bind_desc.Space];
				if (!isDefined(bindings, bind_desc.BindPoint))
				{
					bindings.push_back(bind_desc);
				}
			} else if (bind_desc.Type == D3D_SIT_UAV_RWTYPED || bind_desc.Type == D3D_SIT_UAV_RWSTRUCTURED)
			{
				auto &bindings = uav_by_space[bind_desc.Space];
				if (!isDefined(bindings, bind_desc.BindPoint))
				{
					bindings.push_back(bind_desc);
				}
			}

			// texture, samplers, structured buffers, uav as separate descriptor tables
			// TODO:
		}
	}

	// CBVs
	/*
	for (const auto& [space, bindings] : cbv_by_space)
	{
	for (const auto& [bind_point, resource] : bindings.binding_by_bind)
	{
	auto &param = params.emplace_back();
	//param.InitAsConstants(cb_desc.Size / 4, bind_desc.BindPoint, bind_desc.Space, visibility);
	param.InitAsConstantBufferView(bind_point, space, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
	}
	}
	*/

	// Create descriptor ranges for continuous register blocks
	auto createDescriptorRanges = [](const std::vector<D3D12_SHADER_INPUT_BIND_DESC> &resources, D3D12_DESCRIPTOR_RANGE_TYPE range_type) -> std::pair<int, CD3DX12_DESCRIPTOR_RANGE1 *>
	{
		if (resources.empty()) return std::make_pair(0, descriptor_ranges.data());

		// Sort resources by BindPoint to identify continuous ranges
		std::vector<D3D12_SHADER_INPUT_BIND_DESC> sorted_resources = resources;
		std::sort(sorted_resources.begin(), sorted_resources.end(), [](const D3D12_SHADER_INPUT_BIND_DESC& a, const D3D12_SHADER_INPUT_BIND_DESC& b) {
			return a.BindPoint < b.BindPoint;
		});


		// Create continuous descriptor ranges
		int start = sorted_resources.front().BindPoint;
		int count = 1;

		int ranges_count = 1;

		for (size_t i = 1; i < sorted_resources.size(); ++i)
		{
			if (sorted_resources[i].BindPoint == sorted_resources[i - 1].BindPoint + 1)
			{
				// Continue the current range
				count++;
			}
			else
			{
				// Finish the current range and start a new one
				auto &range = descriptor_ranges.emplace_back();
				range.Init(range_type, count, start, sorted_resources.front().Space, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
				start = sorted_resources[i].BindPoint;
				count = 1;
				ranges_count++;
			}
		}

		// Add the last range
		CD3DX12_DESCRIPTOR_RANGE1 range;
		range.Init(range_type, count, start, sorted_resources.front().Space, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
		descriptor_ranges.push_back(range);

		CD3DX12_DESCRIPTOR_RANGE1 *start_range_ptr = (&descriptor_ranges.back()) - (count - 1);

		return std::make_pair(ranges_count, start_range_ptr);
	};


	descriptor_ranges.reserve(1000);
	descriptor_ranges.clear();

	// Add descriptor tables to root parameters
	auto addDescriptorTables = [&params, createDescriptorRanges](const std::map<int, std::vector<D3D12_SHADER_INPUT_BIND_DESC>> &bindings_map, D3D12_DESCRIPTOR_RANGE_TYPE range_type, TableInfo &table_info)
	{
		for (const auto& [space, bindings] : bindings_map)
		{
			// Other than 0 spaces are specialized
			if (space != 0)
				continue;

			auto ranges = createDescriptorRanges(bindings, range_type);

			if (ranges.first == 0)
				continue;

			auto *ranges_start = &descriptor_ranges[descriptor_ranges.size() - 1];
			ranges_start -= ranges.first - 1;

			if (space == 0)
			{
				table_info.table_index = params.size();
				table_info.begin_register = ranges_start[0].BaseShaderRegister;
				table_info.end_register = ranges_start[0].BaseShaderRegister + ranges_start[0].NumDescriptors;
			}

			// For each range create separate descriptor table
			for (int i = 0; i < ranges.first; i++)
			{
				CD3DX12_ROOT_PARAMETER1 param;
				auto *range = &ranges_start[i];
				param.InitAsDescriptorTable(1, range, D3D12_SHADER_VISIBILITY_ALL);

				params.push_back(param);
			}
		}
	};


	// Create descriptor tables for each resource type
	addDescriptorTables(srv_by_space, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, binding_info.srv_table);
	addDescriptorTables(uav_by_space, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, binding_info.uav_table);
	addDescriptorTables(sampler_by_space, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, binding_info.samplers_table);

	// Bindless
	const int BINDLESS_COUNT = 100'000 / 2;
	D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
	{
		CD3DX12_DESCRIPTOR_RANGE1 &range = descriptor_ranges.emplace_back();
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, BINDLESS_COUNT, 0, 1, flags);

		CD3DX12_ROOT_PARAMETER1 param;
		param.InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_ALL);

		binding_info.srv_bindless = params.size();
		params.push_back(param);
	}

	{
		CD3DX12_DESCRIPTOR_RANGE1 &range = descriptor_ranges.emplace_back();
		range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, BINDLESS_COUNT, 0, 1, flags);

		CD3DX12_ROOT_PARAMETER1 param;
		param.InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_ALL);

		binding_info.samplers_bindless = params.size();
		params.push_back(param);
	}

	return params;
}
