#pragma once
#include "RHI/DynamicRHI.h"
#include "Rendering/GlobalPipeline.h"

class RendererBase
{
public:
	// Create resources there
	RendererBase() = default;

	// Free resources
	virtual ~RendererBase() {}

	// Add commands to command buffer
	virtual void fillCommandBuffer(RHICommandList *cmd_list) {}
private:
};

