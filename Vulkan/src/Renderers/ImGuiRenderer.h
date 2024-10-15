#pragma once
#include <GLFW/glfw3.h>

#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"

class ImGuiRenderer : public RendererBase
{
public:
	ImGuiRenderer(GLFWwindow *window);
	virtual ~ImGuiRenderer();

	void begin();
	void fillCommandBuffer(CommandBuffer &command_buffer) override;
private:
	VkDescriptorPool descriptor_pool;
};

