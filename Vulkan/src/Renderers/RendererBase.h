#pragma once
#include "RHI/VkWrapper.h"


class RendererBase
{
public:
	// Create resources there
	RendererBase() = default;

	// Free resources
	virtual ~RendererBase() {}

	// Add commands to command buffer
	virtual void fillCommandBuffer(CommandBuffer &command_buffer) {}
private:
};

