#pragma once
#include "Buffer.h"
#include "Device.h"
#include "Log.h"
#include "Mesh.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Swapchain.h"
#include "Texture.h"
#include "VkWrapper.h"
#include "renderers/MeshRenderer.h"
#include "renderers/ImGuiRenderer.h"

class Application
{
public:
	bool framebufferResized = false;
public:
	Application();
	void Run();
private:
	void UpdateUniformBuffer(uint32_t currentImage);
	void RecordCommandBuffer(CommandBuffer& command_buffer, uint32_t image_index);
	void cleanup();
	void CleanupSwapchain();

	void RecreateSwapchain();

	void InitDepth();
	void InitSyncObjects();
private:
	GLFWwindow* window;

	std::vector<std::shared_ptr<Texture>> depthStencilImages;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	int currentFrame = 0;

	std::shared_ptr<MeshRenderer> mesh_renderer;
	std::shared_ptr<ImGuiRenderer> imgui_renderer;
};