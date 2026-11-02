#pragma once
#include "FrameGraphRHIResources.h"
#include "FrameGraphPass.h"

class FrameGraph;

class RenderPassBuilder
{
public:
	FrameGraphTextureId createTexture(GraphicsResourceName name, TextureDescription desc);
	FrameGraphTextureId createTexture(GraphicsResourceName name, uint32_t width, uint32_t height, Format format);
	FrameGraphBufferId createBuffer(GraphicsResourceName name, size_t stride, size_t count, BufferUsage usage_flags);

	bool createPersistentTexture(GraphicsResourceName name, TextureDescription desc);
	bool createPersistentTexture(GraphicsResourceName name, uint32_t width, uint32_t height, Format format);
	bool createPersistentBuffer(GraphicsResourceName name, size_t stride, size_t count, BufferUsage usage_flags);

	FrameGraphTextureId readTexture(GraphicsResourceName texture);
	FrameGraphTextureId readDepthTexture(GraphicsResourceName texture);
	FrameGraphTextureId readCopyTexture(GraphicsResourceName texture);

	FrameGraphTextureId writeTexture(GraphicsResourceName name);
	FrameGraphTextureId writeUAVTexture(GraphicsResourceName name);
	FrameGraphTextureId writeCopyTexture(GraphicsResourceName name);

	FrameGraphBufferId writeBuffer(GraphicsResourceName name);
	FrameGraphBufferId writeCopyBuffer(GraphicsResourceName name);
	FrameGraphBufferId readBuffer(GraphicsResourceName name);
	FrameGraphBufferId readIndirectArgsBuffer(GraphicsResourceName name);
	FrameGraphBufferId readVertexBuffer(GraphicsResourceName name);

	bool isTextureCreated(GraphicsResourceName name);
	bool isBufferCreated(GraphicsResourceName name);
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
