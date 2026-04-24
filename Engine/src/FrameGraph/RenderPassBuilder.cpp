#include "pch.h"
#include "FrameGraph.h"

RenderPassBuilder::RenderPassBuilder(FrameGraph &frameGraph, RenderPassNode &renderpass_node)
	: frameGraph(frameGraph), renderpass_node(renderpass_node)
{}

FrameGraphTextureId RenderPassBuilder::createTexture(GraphicsResourceName name, uint32_t width, uint32_t height, Format format)
{
	TextureDescription desc;
	desc.width = width;
	desc.height = height;
	desc.format = format;
	return createTexture(name, desc);
}

FrameGraphTextureId RenderPassBuilder::createTexture(GraphicsResourceName name, TextureDescription desc)
{
	FrameGraphTextureId resource_id = frameGraph.createTextureResource(name, desc);
	renderpass_node.texture_creates.emplace_back(resource_id);
	return resource_id;
}

FrameGraphBufferId RenderPassBuilder::createBuffer(GraphicsResourceName name, size_t stride, size_t count, BufferUsage usage)
{
	BufferDescription desc;
	desc.size = stride * count;
	desc.usage = usage;
	desc.use_staging_buffer = true;
	desc.storage_stride = stride;

	FrameGraphBufferId resource_id = frameGraph.createBufferResource(name, desc);
	renderpass_node.buffer_creates.emplace_back(resource_id);
	return resource_id;
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

FrameGraphTextureId RenderPassBuilder::writeTexture(GraphicsResourceName name)
{
	auto resource_id = frameGraph.texture_name_to_id[name];
	frameGraph.all_textures[resource_id.id].desc.usage_flags |= TEXTURE_USAGE_ATTACHMENT;
	return declare_texture_write(resource_id, ResourceState::RENDER_TARGET);
}

FrameGraphTextureId RenderPassBuilder::writeUAVTexture(GraphicsResourceName name)
{
	auto resource_id = frameGraph.texture_name_to_id[name];
	frameGraph.all_textures[resource_id.id].desc.usage_flags |= TEXTURE_USAGE_STORAGE;
	return declare_texture_write(resource_id, ResourceState::UAV);
}

FrameGraphBufferId RenderPassBuilder::writeBuffer(GraphicsResourceName name)
{
	auto resource_id = frameGraph.buffer_name_to_id[name];
	frameGraph.all_buffers[resource_id.id].desc.usage |= BufferUsage::SHADER_WRITE_BUFFER;
	return declare_buffer_write(resource_id, ResourceState::UAV);
}

FrameGraphBufferId RenderPassBuilder::readBuffer(GraphicsResourceName name)
{
	FrameGraphBufferId buffer = frameGraph.buffer_name_to_id[name];
	frameGraph.all_buffers[buffer.id].desc.usage |= BufferUsage::SHADER_READ_BUFFER;
	return declare_buffer_read(buffer, ResourceState::SHADER_READ);
}

FrameGraphBufferId RenderPassBuilder::readIndirectArgsBuffer(GraphicsResourceName name)
{
	FrameGraphBufferId buffer = frameGraph.buffer_name_to_id[name];
	frameGraph.all_buffers[buffer.id].desc.usage |= BufferUsage::INDIRECT_ARGS_BUFFER;
	return declare_buffer_read(buffer, ResourceState::INDIRECT_ARGS);
}

FrameGraphBufferId RenderPassBuilder::readVertexBuffer(GraphicsResourceName name)
{
	FrameGraphBufferId buffer = frameGraph.buffer_name_to_id[name];
	frameGraph.all_buffers[buffer.id].desc.usage |= BufferUsage::VERTEX_BUFFER;
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
