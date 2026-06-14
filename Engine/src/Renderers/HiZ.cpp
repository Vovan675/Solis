#include "pch.h"
#include "HiZ.h"
#include "Rendering/Renderer.h"
#include "Rendering/GlobalPipeline.h"

namespace
{
constexpr uint32_t HIZ_THREADGROUP_SIZE = 32;
}

void HiZ::createOrImport(FrameGraph &fg, RHITextureRef &texture, GraphicsResourceName name, glm::ivec2 size, uint32_t layers)
{
	glm::ivec2 mip_dimensions = glm::max(glm::ceil(glm::log2(glm::vec2(size))), glm::vec2(1.0f));
	uint32_t mip_levels = std::max(mip_dimensions.x, mip_dimensions.y);
	uint32_t width = 1u << (mip_dimensions.x - 1);
	uint32_t height = 1u << (mip_dimensions.y - 1);

	bool need_realloc = !texture
		|| texture->getWidth() != width
		|| texture->getHeight() != height
		|| texture->getMipLevels() != mip_levels
		|| texture->getArrayLevels() != layers;

	if (need_realloc)
	{
		TextureDescription desc;
		desc.format = FORMAT_R32_SFLOAT;
		desc.width = width;
		desc.height = height;
		desc.mip_levels = mip_levels;
		desc.array_levels = layers;
		desc.usage_flags = TEXTURE_USAGE_STORAGE;
		desc.filtering = FILTER_NEAREST;
		texture = gDynamicRHI->createTexture(desc);
		texture->fill();
		texture->setDebugName(name.name);
	}

	fg.importTexture(name, texture);
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

		gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/depth_hiz.hlsl", COMPUTE_SHADER, "CSMain",
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

			glm::ivec2 dispatch_size = glm::max(glm::vec2(1, 1), glm::ceil(glm::vec2(hiz->getSize(mip)) / float(HIZ_THREADGROUP_SIZE)));
			cmd_list->dispatch(dispatch_size.x, dispatch_size.y, 1);
			hiz->transitLayout(cmd_list, TEXTURE_LAYOUT_UAV);
		}
	});
}
