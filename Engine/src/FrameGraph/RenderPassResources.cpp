#include "pch.h"
#include "FrameGraph.h"

RenderPassResources::RenderPassResources(FrameGraph &frameGraph, const RenderPassNode &pass)
	: frameGraph(frameGraph), pass(pass)
{}

RHITexture *RenderPassResources::getTexture(GraphicsResourceName name) const
{
	FrameGraphTextureId resource_id = frameGraph.texture_name_to_id[name];
	return frameGraph.all_textures[resource_id.id].resource;
}

RHIBuffer *RenderPassResources::getBuffer(GraphicsResourceName name) const
{
	FrameGraphBufferId resource_id = frameGraph.buffer_name_to_id[name];
	return frameGraph.all_buffers[resource_id.id].resource;
}

uint32_t RenderPassResources::getReadTexture(GraphicsResourceName name) const
{
	return getTexture(name)->getShaderResourceView()->getBindlessIndex();
}

uint32_t RenderPassResources::getReadWriteTexture(GraphicsResourceName name) const
{
	return getTexture(name)->getUnorderedAccessView()->getBindlessIndex();
}

uint32_t RenderPassResources::getReadBuffer(GraphicsResourceName name) const
{
	return getBuffer(name)->getShaderResourceView()->getBindlessIndex();
}

uint32_t RenderPassResources::getReadWriteBuffer(GraphicsResourceName name) const
{
	return getBuffer(name)->getUnorderedAccessView()->getBindlessIndex();
}

bool RenderPassResources::has(GraphicsResourceName name) const
{
	return frameGraph.texture_name_to_id.contains(name);
}
