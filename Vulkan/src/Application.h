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
#include "renderers/LutRenderer.h"
#include "renderers/IrradianceRenderer.h"
#include "renderers/PrefilterRenderer.h"
#include "renderers/CubeMapRenderer.h"
#include "renderers/MeshRenderer.h"
#include "renderers/ImGuiRenderer.h"
#include "renderers/DefferedLightingRenderer.h"
#include "renderers/DefferedCompositeRenderer.h"
#include "renderers/PostProcessingRenderer.h"
#include "renderers/DebugRenderer.h"
#include "renderers/SSAORenderer.h"
#include "VulkanApp.h"

class Application : public VulkanApp
{
public:
	Application();
protected:
	void createRenderTargets();
	void update(float delta_time) override;
	void updateBuffers(float delta_time, uint32_t image_index) override;
	void recordCommands(CommandBuffer &command_buffer, uint32_t image_index) override;
	void cleanupResources() override;
	void onSwapchainRecreated(int width, int height) override;
private:
	void render_GBuffer(CommandBuffer &command_buffer, uint32_t image_index);
	void render_lighting(CommandBuffer &command_buffer, uint32_t image_index);
	void render_ssao(CommandBuffer &command_buffer, uint32_t image_index);
	void render_deffered_composite(CommandBuffer &command_buffer, uint32_t image_index);
	void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) override;

private:
	bool debug_rendering = false;
	bool is_first_frame = true;

	std::shared_ptr<Texture> ibl_irradiance;
	std::shared_ptr<Texture> ibl_prefilter;
	std::shared_ptr<Texture> ibl_brdf_lut;

	std::shared_ptr<LutRenderer> lut_renderer;
	std::shared_ptr<IrradianceRenderer> irradiance_renderer;
	std::shared_ptr<PrefilterRenderer> prefilter_renderer;

	std::shared_ptr<CubeMapRenderer> cubemap_renderer;
	std::shared_ptr<MeshRenderer> mesh_renderer;
	std::shared_ptr<MeshRenderer> mesh_renderer2;
	std::shared_ptr<DefferedLightingRenderer> defferred_lighting_renderer;
	std::shared_ptr<DefferedCompositeRenderer> deffered_composite_renderer;
	std::shared_ptr<PostProcessingRenderer> post_renderer;
	std::shared_ptr<DebugRenderer> debug_renderer;
	std::shared_ptr<ImGuiRenderer> imgui_renderer;
	std::shared_ptr<SSAORenderer> ssao_renderer;


	std::vector<std::shared_ptr<RendererBase>> renderers;
	std::vector<std::shared_ptr<RendererBase>> entities_renderers;


	std::shared_ptr<Camera> camera;
};