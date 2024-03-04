#pragma once
#include <VkWrapper.h>


class RendererBase
{
public:
	// Create resources there
	RendererBase() {}

	// Free resources
	virtual ~RendererBase() {}

	// Add commands to command buffer
	virtual void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) {}

	// Update uniform buffer there if needed
	virtual void updateUniformBuffer(uint32_t image_index) {}
private:
};

