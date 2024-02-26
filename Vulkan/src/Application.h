#pragma once
#include "Log.h"
#include "Device.h"
#include "Buffer.h"
#include "Shader.h"
#include "Swapchain.h"
#include "Texture.h"
#include "Mesh.h"
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
	void RecordCommandBuffer(CommandBuffer& commandBuffer, uint32_t imageIndex);
	void Cleanup();
	void CleanupSwapchain();

	void RecreateSwapchain();

	void InitSwapchain();
	void InitRenderpass();
	void InitShaders();
	void InitDescriptorLayout();
	void InitPipeline();
	void InitFramebuffers();
	void InitMesh();
	void InitColor();
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

	VkRenderPass renderpass;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

	std::vector<VkFramebuffer> swapchainFramebuffers;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	int currentFrame = 0;

	VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_8_BIT;

	std::shared_ptr<Texture> colorImage;
	std::shared_ptr<Texture> depthStencilImage;
	std::shared_ptr<Texture> image;

	std::shared_ptr<Engine::Mesh> modelMesh;
	std::vector<std::shared_ptr<Buffer>> uniformBuffers;
	std::vector<void*> uniform_buffer_mapped;

	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
};