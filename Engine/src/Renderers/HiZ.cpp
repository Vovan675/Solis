#include "pch.h"
#include "HiZ.h"
#include "Rendering/Renderer.h"
#include "Rendering/GlobalPipeline.h"

namespace
{
constexpr uint32_t HIZ_THREADGROUP_SIZE = 32;
}

TextureDescription HiZ::createDescription(glm::ivec2 size, uint32_t layers)
{
	glm::ivec2 mip_dimensions = glm::max(glm::ceil(glm::log2(glm::vec2(size))), glm::vec2(1.0f));

	TextureDescription desc;
	desc.format = Format::R32_SFLOAT;
	desc.width = 1u << (mip_dimensions.x - 1);
	desc.height = 1u << (mip_dimensions.y - 1);
	desc.mip_levels = std::max(mip_dimensions.x, mip_dimensions.y);
	desc.array_layers = layers;
	desc.usage_flags = TextureUsage::UNORDERED_ACCESS;
	return desc;
}

void HiZ::build(FrameGraph &fg, GraphicsResourceName hiz_name, GraphicsResourceName depth_name, uint32_t layer, bool is_depth_reverse_z)
{
	fg.addCallbackPass("HiZ Generation",
	[hiz_name, depth_name](RenderPassBuilder &builder)
	{
		builder.writeUAVTexture(hiz_name);
		builder.readTexture(depth_name);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		RHITexture *hiz = resources.getTexture(hiz_name);
		RHITexture *depth = resources.getTexture(depth_name);

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/depth_hiz.hlsl", ShaderType::COMPUTE, "CSMain",
											  {
												  {"THREADGROUP_SIZE", std::to_string(HIZ_THREADGROUP_SIZE).c_str()}
											  }));
		gGlobalPipeline->flushAndBind(cmd_list);

		for (int mip = 0; mip < hiz->getMipLevels(); mip++)
		{
			struct
			{
				uint32_t output_tex_id;
				uint32_t depth_tex_id;
				glm::ivec2 texture_size;
				uint32_t convert_reverse_z;
			} constants;
			constants.depth_tex_id = (mip == 0)
				? depth->getShaderResourceView(0, layer)->getBindlessIndex()
				: hiz->getShaderResourceView(mip - 1, layer)->getBindlessIndex();
			constants.output_tex_id = hiz->getUnorderedAccessView(mip, layer)->getBindlessIndex();
			constants.texture_size = hiz->getSize(mip);
			constants.convert_reverse_z = (is_depth_reverse_z && mip == 0) ? 1u : 0u;

			gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));

			cmd_list->dispatch(Math::divideRoundUp(hiz->getSize(mip).x, HIZ_THREADGROUP_SIZE), Math::divideRoundUp(hiz->getSize(mip).y, HIZ_THREADGROUP_SIZE), 1);
			hiz->transitState(cmd_list, ResourceState::UNORDERED_ACCESS);
		}
	});
}
