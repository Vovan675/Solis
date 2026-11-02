#include "pch.h"
#include "VulkanShader.h"
#include "VulkanDynamicRHI.h"

VulkanShader::VulkanShader(const eastl::wstring &path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines)
	: RHIShader(path, type, entry_point, defines)
{
	recompile();
}

void VulkanShader::destroy()
{
	if (handle)
	{
		vkDestroyShaderModule(VulkanUtils::getNativeRHI()->device->logicalHandle, handle, nullptr);
		handle = nullptr;
	}
}

void VulkanShader::load_bytecode(IDxcBlob *bytecode)
{
	destroy();

	VkShaderModuleCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = bytecode->GetBufferSize();
	create_info.pCode = (const uint32_t *)bytecode->GetBufferPointer();
	vkCreateShaderModule(VulkanUtils::getNativeRHI()->device->logicalHandle, &create_info, nullptr, &handle);
	reflection = spv_reflect::ShaderModule(create_info.codeSize, create_info.pCode);
}

eastl::vector<Descriptor> VulkanShader::getDescriptors(const eastl::fixed_vector<RHIShader *, 3> &shaders)
{
	eastl::vector<Descriptor> descriptors;

	for (RHIShader *rhi_shader : shaders)
	{
		VulkanShader *shader = static_cast<VulkanShader *>(rhi_shader);
		DescriptorStage stage;
		switch (shader->type)
		{
			case ShaderType::VERTEX:
				stage = DescriptorStage::VERTEX;
				break;
			case ShaderType::FRAGMENT:
				stage = DescriptorStage::FRAGMENT;
				break;
			case ShaderType::COMPUTE:
				stage = DescriptorStage::COMPUTE;
				break;
			case ShaderType::RAY_GENERATION:
				stage = DescriptorStage::RAY_GENERATION;
				break;
			case ShaderType::MISS:
				stage = DescriptorStage::MISS;
				break;
			case ShaderType::CLOSEST_HIT:
				stage = DescriptorStage::CLOSEST_HIT;
				break;
		}

		spv_reflect::ShaderModule &sm = shader->reflection;


		auto isDefined = [&descriptors](DescriptorType type, int set, int binding)
		{
			for (const auto &desc : descriptors)
			{
				if (desc.type == type && desc.set == set && desc.binding == binding)
					return true;
			}
			return false;
		};

		uint32_t count;
		// Push Constants
		{
			sm.EnumeratePushConstants(&count, nullptr);

			eastl::vector<SpvReflectBlockVariable *> blocks(count);
			sm.EnumeratePushConstants(&count, blocks.data());

			for (auto *pc : blocks)
			{
				uint32_t size = pc->size;
				int set = 0;
				int binding = 0;

				if (!isDefined(DescriptorType::PUSH_CONSTANT, set, binding))
					auto &desc = descriptors.emplace_back(DescriptorType::PUSH_CONSTANT, set, binding, size, stage, pc->name);
			}
		}


		// CBV SRV UAV Bindings
		{
			sm.EnumerateDescriptorBindings(&count, nullptr);

			eastl::vector<SpvReflectDescriptorBinding *> bindings(count);
			sm.EnumerateDescriptorBindings(&count, bindings.data());

			for (auto *binding : bindings)
			{
				// Constant buffers (cbuffer of ConstantBuffer<T>)
				if (binding->resource_type == SPV_REFLECT_RESOURCE_FLAG_CBV)
				{
					uint32_t size = binding->block.size;
					if (!isDefined(DescriptorType::UNIFORM_BUFFER, binding->set, binding->binding))
						auto &desc = descriptors.emplace_back(DescriptorType::UNIFORM_BUFFER, binding->set, binding->binding, size, stage, binding->name);
				}
				// SRV (StructuredBuffer, Texture2D, ...)
				else if (binding->resource_type == SPV_REFLECT_RESOURCE_FLAG_SAMPLER)
				{
					uint32_t size = binding->block.size;
					//if (!isDefined(DescriptorType::SAMPLER, binding->set, binding->binding))
					//	auto &desc = descriptors.emplace_back(DescriptorType::SAMPLER, binding->set, binding->binding, size, stage, binding->name);
				}
				else if (binding->resource_type == SPV_REFLECT_RESOURCE_FLAG_SRV)
				{
					if (binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
					{
						uint32_t size = binding->block.size;
						if (!isDefined(DescriptorType::SAMPLED_IMAGE, binding->set, binding->binding))
							auto &desc = descriptors.emplace_back(DescriptorType::SAMPLED_IMAGE, binding->set, binding->binding, size, stage, binding->name);
					} else if (binding->descriptor_type  = SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
					{
						uint32_t size = binding->block.size;
						if (!isDefined(DescriptorType::ACCELERATION_STRUCTURE, binding->set, binding->binding))
							auto &desc = descriptors.emplace_back(DescriptorType::ACCELERATION_STRUCTURE, binding->set, binding->binding, size, stage, binding->name);
					}
				}
				else if (binding->resource_type == SPV_REFLECT_RESOURCE_FLAG_UAV)
				{
					if (binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE)
					{
						uint32_t size = binding->block.size;
						if (!isDefined(DescriptorType::STORAGE_IMAGE, binding->set, binding->binding))
							auto &desc = descriptors.emplace_back(DescriptorType::STORAGE_IMAGE, binding->set, binding->binding, size, stage, binding->name);
					} else if (binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER)
					{
						uint32_t size = binding->block.size;
						if (!isDefined(DescriptorType::STORAGE_BUFFER, binding->set, binding->binding))
							auto &desc = descriptors.emplace_back(DescriptorType::STORAGE_BUFFER, binding->set, binding->binding, size, stage, binding->name);
					}

				}
			}
		}

	}

	return descriptors;
}

eastl::vector<VkPushConstantRange> VulkanShader::getPushConstantRanges(eastl::vector<Descriptor> &descriptors)
{
	// Create desciptor ranges based on descriptors
	const auto stage_to_vk = [](DescriptorStage stage) {
		VkShaderStageFlags vk_stage = 0;
		if (hasAnyFlags(stage, DescriptorStage::VERTEX))
			vk_stage |= VK_SHADER_STAGE_VERTEX_BIT;
		if (hasAnyFlags(stage, DescriptorStage::FRAGMENT))
			vk_stage |= VK_SHADER_STAGE_FRAGMENT_BIT;
		if (hasAnyFlags(stage, DescriptorStage::COMPUTE))
			vk_stage |= VK_SHADER_STAGE_COMPUTE_BIT;
		if (hasAnyFlags(stage, DescriptorStage::RAY_GENERATION))
			vk_stage |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		if (hasAnyFlags(stage, DescriptorStage::MISS))
			vk_stage |= VK_SHADER_STAGE_MISS_BIT_KHR;
		if (hasAnyFlags(stage, DescriptorStage::CLOSEST_HIT))
			vk_stage |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		return vk_stage;
	};

	eastl::vector<VkPushConstantRange> ranges;
	for (auto &desc : descriptors)
	{
		if (desc.type == DescriptorType::PUSH_CONSTANT)
		{
			VkPushConstantRange range;
			range.stageFlags = stage_to_vk(desc.stage);
			range.offset = desc.first_member_offset;
			range.size = desc.size;
			ranges.push_back(range);
		}
	}

	return ranges;
}