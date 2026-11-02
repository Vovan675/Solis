#include "pch.h"
#include "FrameGraph.h"

RenderPassBuilder::RenderPassBuilder(FrameGraph &frameGraph, RenderPassNode &renderpass_node)
	: frameGraph(frameGraph), renderpass_node(renderpass_node)
{}

static TextureDescription create_texture_desc(uint32_t width, uint32_t height, Format format)
{
	TextureDescription desc;
	desc.width = width;
	desc.height = height;
	desc.format = format;
	return desc;
}

static BufferDescription create_buffer_desc(size_t stride, size_t count, BufferUsage usage_flags)
{
	BufferDescription desc;
	desc.size = stride * count;
	desc.usage_flags = usage_flags;
	desc.stride = stride;
	return desc;
}

FrameGraphTextureId RenderPassBuilder::createTexture(GraphicsResourceName name, TextureDescription desc)
{
	FrameGraphTextureId resource_id = frameGraph.createTextureResource(name, desc);
	renderpass_node.texture_creates.emplace_back(resource_id);
	return resource_id;
}

FrameGraphTextureId RenderPassBuilder::createTexture(GraphicsResourceName name, uint32_t width, uint32_t height, Format format)
{
	return createTexture(name, create_texture_desc(width, height, format));
}

FrameGraphBufferId RenderPassBuilder::createBuffer(GraphicsResourceName name, size_t stride, size_t count, BufferUsage usage_flags)
{
	FrameGraphBufferId resource_id = frameGraph.createBufferResource(name, create_buffer_desc(stride, count, usage_flags));
	renderpass_node.buffer_creates.emplace_back(resource_id);
	return resource_id;
}

bool RenderPassBuilder::createPersistentTexture(GraphicsResourceName name, TextureDescription desc)
{
	return frameGraph.createPersistentTexture(name, desc);
}

bool RenderPassBuilder::createPersistentTexture(GraphicsResourceName name, uint32_t width, uint32_t height, Format format)
{
	return createPersistentTexture(name, create_texture_desc(width, height, format));
}

bool RenderPassBuilder::createPersistentBuffer(GraphicsResourceName name, size_t stride, size_t count, BufferUsage usage_flags)
{
	return frameGraph.createPersistentBuffer(name, create_buffer_desc(stride, count, usage_flags));
}

FrameGraphTextureId RenderPassBuilder::readTexture(GraphicsResourceName texture)
{
	auto resource_id = frameGraph.texture_name_to_id[texture];
	renderpass_node.texture_usage[resource_id] = ResourceState::SHADER_RESOURCE;
	return renderpass_node.texture_reads.emplace_back(resource_id);
}

FrameGraphTextureId RenderPassBuilder::readDepthTexture(GraphicsResourceName texture)
{
	auto resource_id = frameGraph.texture_name_to_id[texture];
	renderpass_node.texture_usage[resource_id] = ResourceState::DEPTH_READ;
	return renderpass_node.texture_reads.emplace_back(resource_id);
}

FrameGraphTextureId RenderPassBuilder::readCopyTexture(GraphicsResourceName texture)
{
	auto resource_id = frameGraph.texture_name_to_id[texture];
	renderpass_node.texture_usage[resource_id] = ResourceState::COPY_SRC;
	return renderpass_node.texture_reads.emplace_back(resource_id);
}

FrameGraphTextureId RenderPassBuilder::writeTexture(GraphicsResourceName name)
{
	auto resource_id = frameGraph.texture_name_to_id[name];
	frameGraph.all_textures[resource_id.id].desc.usage_flags |= TextureUsage::RENDER_TARGET;
	return declare_texture_write(resource_id, ResourceState::RENDER_TARGET);
}

FrameGraphTextureId RenderPassBuilder::writeUAVTexture(GraphicsResourceName name)
{
	auto resource_id = frameGraph.texture_name_to_id[name];
	frameGraph.all_textures[resource_id.id].desc.usage_flags |= TextureUsage::UNORDERED_ACCESS;
	return declare_texture_write(resource_id, ResourceState::UNORDERED_ACCESS);
}

FrameGraphTextureId RenderPassBuilder::writeCopyTexture(GraphicsResourceName name)
{
	auto resource_id = frameGraph.texture_name_to_id[name];
	return declare_texture_write(resource_id, ResourceState::COPY_DST);
}

FrameGraphBufferId RenderPassBuilder::writeBuffer(GraphicsResourceName name)
{
	auto resource_id = frameGraph.buffer_name_to_id[name];
	frameGraph.all_buffers[resource_id.id].desc.usage_flags |= BufferUsage::UNORDERED_ACCESS;
	return declare_buffer_write(resource_id, ResourceState::UNORDERED_ACCESS);
}

FrameGraphBufferId RenderPassBuilder::writeCopyBuffer(GraphicsResourceName name)
{
	auto resource_id = frameGraph.buffer_name_to_id[name];
	return declare_buffer_write(resource_id, ResourceState::COPY_DST);
}

FrameGraphBufferId RenderPassBuilder::readBuffer(GraphicsResourceName name)
{
	FrameGraphBufferId buffer = frameGraph.buffer_name_to_id[name];
	frameGraph.all_buffers[buffer.id].desc.usage_flags |= BufferUsage::SHADER_RESOURCE;
	return declare_buffer_read(buffer, ResourceState::SHADER_RESOURCE);
}

FrameGraphBufferId RenderPassBuilder::readIndirectArgsBuffer(GraphicsResourceName name)
{
	FrameGraphBufferId buffer = frameGraph.buffer_name_to_id[name];
	frameGraph.all_buffers[buffer.id].desc.usage_flags |= BufferUsage::INDIRECT_ARGS;
	return declare_buffer_read(buffer, ResourceState::INDIRECT_ARGS);
}

FrameGraphBufferId RenderPassBuilder::readVertexBuffer(GraphicsResourceName name)
{
	FrameGraphBufferId buffer = frameGraph.buffer_name_to_id[name];
	frameGraph.all_buffers[buffer.id].desc.usage_flags |= BufferUsage::VERTEX;
	return declare_buffer_read(buffer, ResourceState::VERTEX_BUFFER);
}

bool RenderPassBuilder::isTextureCreated(GraphicsResourceName name)
{
	return frameGraph.texture_name_to_id.contains(name);
}

bool RenderPassBuilder::isBufferCreated(GraphicsResourceName name)
{
	return frameGraph.buffer_name_to_id.contains(name);
}

TextureDescription RenderPassBuilder::getTextureDescription(GraphicsResourceName name)
{
	return frameGraph.getTextureDescription(name);
}

void RenderPassBuilder::setSideEffect(bool side_effect)
{
	renderpass_node.has_side_effect = side_effect;
}

FrameGraphTextureId RenderPassBuilder::declare_texture_write(FrameGraphTextureId texture, ResourceState usage)
{
	if (!renderpass_node.isCreating(texture))
		renderpass_node.texture_reads.push_back(texture);

	renderpass_node.texture_usage[texture] = usage;
	return renderpass_node.texture_writes.emplace_back(texture);
}

FrameGraphBufferId RenderPassBuilder::declare_buffer_write(FrameGraphBufferId buffer, ResourceState usage)
{
	if (!renderpass_node.isCreating(buffer))
		renderpass_node.buffer_reads.push_back(buffer);

	renderpass_node.buffer_usage[buffer] = usage;
	return renderpass_node.buffer_writes.emplace_back(buffer);
}

FrameGraphBufferId RenderPassBuilder::declare_buffer_read(FrameGraphBufferId buffer, ResourceState usage)
{
	renderpass_node.buffer_usage[buffer] = usage;
	return renderpass_node.buffer_reads.emplace_back(buffer);
}
