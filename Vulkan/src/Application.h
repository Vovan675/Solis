#pragma once
#include "RHI/Buffer.h"
#include "RHI/Device.h"
#include "Log.h"
#include "Mesh.h"
#include "RHI/Pipeline.h"
#include "RHI/Shader.h"
#include "RHI/Swapchain.h"
#include "RHI/Texture.h"
#include "RHI/VkWrapper.h"
#include "Input.h"

class GPUResourceManager;

extern Input input;
extern GPUResourceManager gpu_resource_manager;

class Application
{
public:
	Application();
	void run();
protected:
	virtual void update(float delta_time) {}
	virtual void updateBuffers(float delta_time) {}
	virtual void recordCommands(CommandBuffer &command_buffer) {}
	virtual void cleanupResources() {}
private:
	void render(CommandBuffer &command_buffer);

	void cleanup();

	void cleanup_swapchain();
	void recreate_swapchain();
	virtual void onViewportSizeChanged(int width, int height) {}

	void init_sync_objects();

	void framebuffer_resize_callback(GLFWwindow *window, int width, int height) { framebuffer_resized = true; }
	virtual void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {}
protected:
	GLFWwindow *window;
private:
	bool framebuffer_resized = false;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	int current_frame = 0;
};

