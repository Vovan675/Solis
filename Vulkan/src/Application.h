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
#include "renderers/CubeMapRenderer.h"
#include "renderers/MeshRenderer.h"
#include "renderers/ImGuiRenderer.h"
#include "VulkanApp.h"

class Application : public VulkanApp
{
public:
	Application();
protected:
	void update(float delta_time) override;
	void updateBuffers(float delta_time, uint32_t image_index) override;
	void recordCommands(CommandBuffer &command_buffer, uint32_t image_index) override;
	virtual void cleanupResources();
private:
	void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) override;

private:
	std::shared_ptr<CubeMapRenderer> cubemap_renderer;
	std::shared_ptr<MeshRenderer> mesh_renderer;
	std::shared_ptr<MeshRenderer> mesh_renderer2;
	std::shared_ptr<ImGuiRenderer> imgui_renderer;

	std::shared_ptr<Camera> camera;
};