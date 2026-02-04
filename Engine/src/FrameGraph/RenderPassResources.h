#pragma once
#include "FrameGraphRHIResources.h"

class FrameGraph;
class RenderPassNode;

class RenderPassResources
{
public:
	RHITexture *getTexture(GraphicsResourceName name) const;
	RHIBuffer *getBuffer(GraphicsResourceName name) const;

	uint32_t getReadTexture(GraphicsResourceName name) const;
	uint32_t getReadWriteTexture(GraphicsResourceName name) const;

	uint32_t getReadBuffer(GraphicsResourceName name) const;
	uint32_t getReadWriteBuffer(GraphicsResourceName name) const;

	bool has(GraphicsResourceName name) const;

private:
	friend class FrameGraph;
	RenderPassResources(FrameGraph &frameGraph, const RenderPassNode &pass);

	FrameGraph &frameGraph;
	const RenderPassNode &pass;
};
