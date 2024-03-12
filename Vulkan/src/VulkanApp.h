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

class VulkanApp
{
public:
	VulkanApp();
	void run();
protected:
	virtual void update(float delta_time) {}
	virtual void updateBuffers(float delta_time, uint32_t image_index) {}
	virtual void recordCommands(CommandBuffer &command_buffer, uint32_t image_index) {}
	virtual void cleanupResources() {}
private:
	void render(CommandBuffer &command_buffer, uint32_t image_index);

	void cleanup();

	void cleanup_swapchain();
	void recreate_swapchain();
	virtual void onSwapchainRecreated(int width, int height) {}

	void init_sync_objects();

	void framebuffer_resize_callback(GLFWwindow *window, int width, int height) { framebuffer_resized = true; }
	virtual void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {}
protected:
	GLFWwindow *window;
private:
	bool framebuffer_resized = false;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	int currentFrame = 0;
};

