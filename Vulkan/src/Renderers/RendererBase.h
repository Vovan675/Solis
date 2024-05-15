#pragma once
#include "RHI/VkWrapper.h"


class RendererBase
{
public:
	// Create resources there
	RendererBase() = default;

	// Free resources
	virtual ~RendererBase() {}

	// TODO: make it global
	virtual void reloadShaders() {}

	// Add commands to command buffer
	virtual void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) {}
private:
};

