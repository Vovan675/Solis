#include "pch.h"
#include "VulkanShader.h"
#include "VulkanDynamicRHI.h"


VulkanShader::VulkanShader(const uint32_t *spirvCode, size_t codeSize, VulkanDynamicRHI *rhi, ShaderType type, size_t hash): rhi(rhi), type(type)
{
	this->hash = hash;
	create_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	create_info.codeSize = codeSize;
	create_info.pCode = spirvCode;
	vkCreateShaderModule(VulkanUtils::getNativeRHI()->device->logicalHandle, &create_info, nullptr, &handle);
	reflection = spv_reflect::ShaderModule(codeSize, spirvCode);
}

void VulkanShader::destroy()
{
	auto *rhi = VulkanUtils::getNativeRHI();

	if (handle)
	{
		vkDestroyShaderModule(rhi->device->logicalHandle, handle, nullptr);
		handle = nullptr;
	}
}

std::vector<Descriptor> VulkanShader::getDescriptors(std::vector<VulkanShader *> shaders)
{
	std::vector<Descriptor> descriptors;
	descriptors.clear();

	for (auto &shader : shaders)
	{
		DescriptorStage stage;
		switch (shader->type)
		{
			case VERTEX_SHADER:
				stage = DESCRIPTOR_STAGE_VERTEX_SHADER;
				break;
			case FRAGMENT_SHADER:
				stage = DESCRIPTOR_STAGE_FRAGMENT_SHADER;
				break;
			case COMPUTE_SHADER:
				stage = DESCRIPTOR_STAGE_COMPUTE_SHADER;
				break;
			case RAY_GENERATION_SHADER:
				stage = DESCRIPTOR_STAGE_RAY_GENERATION_SHADER;
				break;
			case MISS_SHADER:
				stage = DESCRIPTOR_STAGE_MISS_SHADER;
				break;
			case CLOSEST_HIT_SHADER:
				stage = DESCRIPTOR_STAGE_CLOSEST_HIT_SHADER;
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

			std::vector<SpvReflectBlockVariable *> blocks(count);
			sm.EnumeratePushConstants(&count, blocks.data());

			for (auto *pc : blocks)
			{
				uint32_t size = pc->size;
				int set = 0;
				int binding = 0;

				if (!isDefined(DESCRIPTOR_TYPE_PUSH_CONSTANT, set, binding))
					auto &desc = descriptors.emplace_back(DESCRIPTOR_TYPE_PUSH_CONSTANT, set, binding, size, stage, pc->name);
			}
		}


		// CBV SRV UAV Bindings
		{
			sm.EnumerateDescriptorBindings(&count, nullptr);

			std::vector<SpvReflectDescriptorBinding *> bindings(count);
			sm.EnumerateDescriptorBindings(&count, bindings.data());

			for (auto *binding : bindings)
			{
				// Constant buffers (cbuffer of ConstantBuffer<T>)
				if (binding->resource_type == SPV_REFLECT_RESOURCE_FLAG_CBV)
				{
					uint32_t size = binding->block.size;
					if (!isDefined(DESCRIPTOR_TYPE_UNIFORM_BUFFER, binding->set, binding->binding))
						auto &desc = descriptors.emplace_back(DESCRIPTOR_TYPE_UNIFORM_BUFFER, binding->set, binding->binding, size, stage, binding->name);
				}
				// SRV (StructuredBuffer, Texture2D, ...)
				else if (binding->resource_type == SPV_REFLECT_RESOURCE_FLAG_SAMPLER)
				{
					uint32_t size = binding->block.size;
					//if (!isDefined(DESCRIPTOR_TYPE_SAMPLER, binding->set, binding->binding))
					//	auto &desc = descriptors.emplace_back(DESCRIPTOR_TYPE_SAMPLER, binding->set, binding->binding, size, stage, binding->name);
				}
				else if (binding->resource_type == SPV_REFLECT_RESOURCE_FLAG_SRV)
				{
					if (binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
					{
						uint32_t size = binding->block.size;
						if (!isDefined(DESCRIPTOR_TYPE_COMBINED_IMAGE, binding->set, binding->binding))
							auto &desc = descriptors.emplace_back(DESCRIPTOR_TYPE_COMBINED_IMAGE, binding->set, binding->binding, size, stage, binding->name);
					}

				}
				else if (binding->resource_type == SPV_REFLECT_RESOURCE_FLAG_UAV)
				{
					if (binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE)
					{
						uint32_t size = binding->block.size;
						if (!isDefined(DESCRIPTOR_TYPE_STORAGE_IMAGE, binding->set, binding->binding))
							auto &desc = descriptors.emplace_back(DESCRIPTOR_TYPE_STORAGE_IMAGE, binding->set, binding->binding, size, stage, binding->name);
					}

				}
			}
		}

	}

	return descriptors;
}

std::vector<VkPushConstantRange> VulkanShader::getPushConstantRanges(std::vector<Descriptor> &descriptors)
{
	// Create desciptor ranges based on descriptors
	const auto stage_to_vk = [](DescriptorStage stage) {
		VkShaderStageFlags vk_stage = 0;
		if (stage & DESCRIPTOR_STAGE_VERTEX_SHADER)
			vk_stage |= VK_SHADER_STAGE_VERTEX_BIT;
		if (stage & DESCRIPTOR_STAGE_FRAGMENT_SHADER)
			vk_stage |= VK_SHADER_STAGE_FRAGMENT_BIT;
		if (stage & DESCRIPTOR_STAGE_COMPUTE_SHADER)
			vk_stage |= VK_SHADER_STAGE_COMPUTE_BIT;
		if (stage & DESCRIPTOR_STAGE_RAY_GENERATION_SHADER)
			vk_stage |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		if (stage & DESCRIPTOR_STAGE_MISS_SHADER)
			vk_stage |= VK_SHADER_STAGE_MISS_BIT_KHR;
		if (stage & DESCRIPTOR_STAGE_CLOSEST_HIT_SHADER)
			vk_stage |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		return vk_stage;
	};

	std::vector<VkPushConstantRange> ranges;
	for (auto &desc : descriptors)
	{
		if (desc.type == DESCRIPTOR_TYPE_PUSH_CONSTANT)
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