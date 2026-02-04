#pragma once
#include "FrameGraphRHIResources.h"
#include "FrameGraphPass.h"

class FrameGraph;

class RenderPassBuilder
{
public:
	FrameGraphTextureId createTexture(GraphicsResourceName name, uint32_t width, uint32_t height, Format format);
	FrameGraphTextureId createTexture(GraphicsResourceName name, TextureDescription desc);
	FrameGraphBufferId createBuffer(GraphicsResourceName name, size_t stride, size_t count, BufferUsage usage);

	FrameGraphTextureId readTexture(GraphicsResourceName texture);
	FrameGraphTextureId readDepthTexture(GraphicsResourceName texture);

	FrameGraphTextureId writeTexture(GraphicsResourceName name);
	FrameGraphTextureId writeUAVTexture(GraphicsResourceName name);

	FrameGraphBufferId writeBuffer(GraphicsResourceName name);
	FrameGraphBufferId readBuffer(GraphicsResourceName name);
	FrameGraphBufferId readIndirectArgsBuffer(GraphicsResourceName name);
	FrameGraphBufferId readVertexBuffer(GraphicsResourceName name);

	bool isTextureCreated(GraphicsResourceName name);
	TextureDescription getTextureDescription(GraphicsResourceName name);
	void setSideEffect(bool side_effect);

private:
	friend class FrameGraph;
	RenderPassBuilder(FrameGraph &frameGraph, RenderPassNode &renderpass_node);

	FrameGraphTextureId declare_texture_write(FrameGraphTextureId texture, ResourceState usage);
	FrameGraphBufferId declare_buffer_write(FrameGraphBufferId buffer, ResourceState usage);
	FrameGraphBufferId declare_buffer_read(FrameGraphBufferId buffer, ResourceState usage);

	FrameGraph &frameGraph;
	RenderPassNode &renderpass_node;
};
