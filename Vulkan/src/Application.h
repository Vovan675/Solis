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
#include "renderers/EntityRenderer.h"
#include "renderers/CubeMapRenderer.h"
#include "renderers/MeshRenderer.h"
#include "renderers/ImGuiRenderer.h"
#include "renderers/PostProcessingRenderer.h"
#include "renderers/QuadRenderer.h"
#include "VulkanApp.h"

class Application : public VulkanApp
{
public:
	Application();
protected:
	void update(float delta_time) override;
	void updateBuffers(float delta_time, uint32_t image_index) override;
	void recordCommands(CommandBuffer &command_buffer, uint32_t image_index) override;
	void cleanupResources() override;
	void onSwapchainRecreated(int width, int height) override;
private:
	void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) override;

private:
	std::shared_ptr<Texture> gbuffer_depth_stencil;
	std::shared_ptr<Texture> gbuffer_albedo;
	std::shared_ptr<Texture> gbuffer_normal;

	std::shared_ptr<EntityRenderer> entity_renderer;
	std::shared_ptr<CubeMapRenderer> cubemap_renderer;
	std::shared_ptr<MeshRenderer> mesh_renderer;
	std::shared_ptr<MeshRenderer> mesh_renderer2;
	std::shared_ptr<ImGuiRenderer> imgui_renderer;
	std::shared_ptr<PostProcessingRenderer> post_renderer;
	std::shared_ptr<QuadRenderer> quad_renderer;

	std::shared_ptr<Camera> camera;
};