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

	void InitShaders();
	void InitDescriptorLayout();
	void InitPipeline();
	void InitMesh();
	void InitDepth();
	void InitTextureImage();
	void InitUniformBuffer();
	void InitDescriptorPool();
	void InitDescriptorSet();
	void InitSyncObjects();
private:
	GLFWwindow* window;

	std::shared_ptr<Shader> vertShader;
	std::shared_ptr<Shader> fragShader;

	std::shared_ptr<Pipeline> pipeline;
	VkDescriptorSetLayout descriptorSetLayout;

	std::vector<std::shared_ptr<Texture>> depthStencilImages;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	int currentFrame = 0;

	std::shared_ptr<Texture> image;

	std::shared_ptr<Engine::Mesh> modelMesh;
	std::vector<std::shared_ptr<Buffer>> uniformBuffers;
	std::vector<void*> uniform_buffer_mapped;

	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
};