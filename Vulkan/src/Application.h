#pragma once
#include "RHI/RHIBuffer.h"
#include "Log.h"
#include "Mesh.h"
#include "RHI/RHIPipeline.h"
#include "RHI/RHIShader.h"
#include "RHI/RHITexture.h"
#include "RHI/DynamicRHI.h"
#include "Input.h"

class GPUResourceManager;

extern Input gInput;
extern GPUResourceManager gGpuResourceManager;

class Application
{
public:
	Application(int argc, char *argv[]);
	void run();
protected:
	virtual void update(float delta_time) {}
	virtual void updateBuffers(float delta_time) {}
	virtual void recordCommands(RHICommandList *cmd_list) {}
	virtual void cleanupResources() {}
private:
	void render(RHICommandList *cmd_list);

	void cleanup();

	void cleanup_swapchain();
	void recreate_swapchain();
	virtual void onViewportSizeChanged(int width, int height) {}

	void framebuffer_resize_callback(GLFWwindow *window, int width, int height) { framebuffer_resized = true; }
	virtual void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {}
protected:
	GLFWwindow *window;
private:
	bool framebuffer_resized = false;
};

