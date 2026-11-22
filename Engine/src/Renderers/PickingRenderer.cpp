#include "pch.h"
#include "PickingRenderer.h"
#include "Rendering/Renderer.h"
#include "FrameGraph/FrameGraphData.h"

PickingRenderer::PickingRenderer()
{
	BufferDescription description;
	description.size = sizeof(uint64_t);
	description.stride = sizeof(uint64_t);
	description.usage_flags = BufferUsage::UNORDERED_ACCESS;
	buffer = gDynamicRHI->createBuffer(description);
	buffer->setDebugName("Pick Buffer");
}

void PickingRenderer::request(glm::vec2 uv, eastl::function<void(uint32_t id)> callback)
{
	this->uv = uv;
	this->callback = eastl::move(callback);
}

uint32_t PickingRenderer::getBufferId() const
{
	return buffer->getUnorderedAccessView()->getBindlessIndex();
}

glm::uvec2 PickingRenderer::getPixel() const
{
	return glm::uvec2(uv * glm::vec2(Renderer::getRenderResolution()));
}

void PickingRenderer::addClearPass(FrameGraph &fg)
{
	if (!isActive())
		return;

	fg.importBuffer(GFXRID(PickBuffer), buffer);
	fg.addCallbackPass("Pick Clear",
	[](RenderPassBuilder &builder)
	{
		builder.writeBuffer(GFXRID(PickBuffer));
	},
	[](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		cmd_list->clearBuffer(resources.getBuffer(GFXRID(PickBuffer)), 0);
	});
}

void PickingRenderer::addReadbackPass(FrameGraph &fg)
{
	if (!isActive())
		return;

	eastl::function<void(uint32_t id)> result_callback = eastl::move(callback);
	callback = nullptr;

	fg.addCallbackPass("Pick Readback",
	[](RenderPassBuilder &builder)
	{
		builder.readCopyBuffer(GFXRID(PickBuffer));
		builder.setSideEffect(true);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		BufferDescription description;
		description.size = sizeof(uint64_t);
		description.memory = BufferMemory::READBACK;
		RHIBufferRef readback = gDynamicRHI->createBuffer(description);
		cmd_list->copyBuffer(resources.getBuffer(GFXRID(PickBuffer)), readback, 0, 0, sizeof(uint64_t));

		gDynamicRHI->runAfterGPUFrame([=]()
		{
			uint64_t *value;
			readback->map((void **)&value);

			uint32_t depth = (*value >> 32);
			uint32_t instance_id = (uint32_t)*value;

			uint32_t result = depth != 0 ? instance_id : NO_HIT;
			readback->unmap();
			result_callback(result);
		});
	});
}
