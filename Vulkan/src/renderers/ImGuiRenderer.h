#pragma once
#include <GLFW/glfw3.h>

#include "RendererBase.h"
#include "Pipeline.h"
#include "Mesh.h"
#include "Texture.h"

class ImGuiRenderer : public RendererBase
{
public:
	ImGuiRenderer(GLFWwindow *window);
	virtual ~ImGuiRenderer();

	void begin();
	void fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index) override;
private:
	VkDescriptorPool descriptor_pool;
};

