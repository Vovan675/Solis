#include "pch.h"
#include "UtilityPasses.h"
#include "Rendering/GlobalPipeline.h"
#include "Utils/Image.h"

void UtilityPasses::addBlitPass(FrameGraph &fg, GraphicsResourceName src, GraphicsResourceName dst, BlitBlend blend, const wchar_t *shader)
{
	fg.addCallbackPass("Blit Pass",
	[&](RenderPassBuilder &builder)
	{
		if (!builder.isTextureCreated(src))
			return;
		builder.readTexture(src);
		builder.writeTexture(dst);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		if (!resources.has(src))
			return;

		cmd_list->setRenderTargets({resources.getTexture(dst)}, nullptr, 0, 0, false);

		gGlobalPipeline->setupGraphicsPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/quad.hlsl", VERTEX_SHADER), gDynamicRHI->createShader(shader, FRAGMENT_SHADER), VertexInputsDescription{}, blend != BLIT_BLEND_OPAQUE, false, CULL_MODE_BACK);
		if (blend == BLIT_BLEND_ALPHA)
			gGlobalPipeline->setBlendMode(BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA, BLEND_OP_ADD, BLEND_ZERO, BLEND_ONE, BLEND_OP_ADD);
		gGlobalPipeline->flushAndBind(cmd_list);

		uint32_t src_tex_id = resources.getReadTexture(src);
		gDynamicRHI->setConstantBufferData(0, &src_tex_id, sizeof(src_tex_id));

		cmd_list->drawInstanced(6, 1, 0, 0);
		cmd_list->resetRenderTargets();
	});
}

void UtilityPasses::addDepthBlitPass(FrameGraph &fg, GraphicsResourceName src, GraphicsResourceName dst, glm::ivec2 dst_size)
{
	fg.addCallbackPass("Depth Blit Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(dst, dst_size.x, dst_size.y, builder.getTextureDescription(src).format);
		builder.writeTexture(dst);
		builder.readTexture(src);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		cmd_list->setRenderTargets({}, resources.getTexture(dst), 0, 0, true);

		gGlobalPipeline->setupGraphicsPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/quad.hlsl", VERTEX_SHADER), gDynamicRHI->createShader(L"shaders/utility/depth_blit.hlsl", FRAGMENT_SHADER), VertexInputsDescription{}, false, true, CULL_MODE_BACK);
		gGlobalPipeline->setDepthFunc(COMPARE_FUNC_ALWAYS);
		gGlobalPipeline->flushAndBind(cmd_list);

		uint32_t src_tex_id = resources.getReadTexture(src);
		gDynamicRHI->setConstantBufferData(0, &src_tex_id, sizeof(src_tex_id));

		cmd_list->drawInstanced(6, 1, 0, 0);
		cmd_list->resetRenderTargets();
	});
}

void UtilityPasses::addReadbackPass(FrameGraph &fg, GraphicsResourceName texture, eastl::function<void(Ref<Image>)> callback)
{
	fg.addCallbackPass("Readback Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.readCopyTexture(texture);
		builder.setSideEffect(true);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		RHITexture *src = resources.getTexture(texture);
		uint32_t width = src->getWidth();
		uint32_t height = src->getHeight();
		uint32_t row_pitch = src->getBufferRowPitch();
		Format format = src->getFormat();

		BufferDescription desc;
		desc.size = row_pitch * height;
		desc.usage = BufferUsage::READBACK_BUFFER;
		desc.use_staging_buffer = false;
		RHIBufferRef buffer = gDynamicRHI->createBuffer(desc);

		cmd_list->copyTextureToBuffer(src, buffer);

		gDynamicRHI->runAfterGPUFrame([=]()
		{
			uint32_t row_size = width * getFormatSize(format);
			eastl::vector<uint8_t> pixels(row_size * height);

			uint8_t *mapped;
			buffer->map((void **)&mapped);
			for (int y = 0; y < height; y++)
				memcpy(pixels.data() + y * row_size, mapped + y * row_pitch, row_size);
			buffer->unmap();

			callback(new Image(width, height, format, eastl::move(pixels)));
		});
	});
}
