#include "pch.h"
#include "Application.h"
#include "Rendering/Renderer.h"
#include "Assets/AssetManager.h"
#include "Scene/Scene.h"
#include "GLFW/glfw3native.h"
#include "FrameGraph/TransientResources.h"
#include "Physics/PhysXWrapper.h"

#include "RHI/Vulkan/VulkanDynamicRHI.h"
#include "RHI/DX12/DX12DynamicRHI.h"
#include "RHI/DX12/DX12Utils.h"

#include "Demos/CubesDemo.h"
#include "Demos/TowerGame.h"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#include <imgui/ImGuiWrapper.h>
#include <imgui.h>
#include "Tracy.hpp"

Input gInput;
DynamicRHI *gDynamicRHI = nullptr;
GlobalPipeline *gGlobalPipeline = nullptr;

static TowerGame tower_game;

Application::Application(int argc, char *argv[])
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //FUCK OFF OPENGL
	window = glfwCreateWindow(1920, 1080, "Vulkan Renderer", nullptr, nullptr);
	glfwSwapInterval(0);

	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, [](GLFWwindow *window, int width, int height)
	{
		static_cast<Application *>(glfwGetWindowUserPointer(window))->framebuffer_resize_callback(window, width, height);
	});

	glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		static_cast<Application *>(glfwGetWindowUserPointer(window))->key_callback(window, key, scancode, action, mods);
	});

	HWND hwnd = glfwGetWin32Window(window);
	BOOL is_dark_mode = true;
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &is_dark_mode, sizeof(is_dark_mode));

	gInput.init(window);
	Log::Init();

	GraphicsAPI gapi = GRAPHICS_API_NONE;

	for (int i = 1; i < argc; i++)
	{
		std::string arg = argv[i];
		if (arg == "-rhi")
		{
			if (i + 1 < argc)
			{
				std::string api = argv[i + 1];
				if (api == "vk" || api == "vulkan")
					gapi = GRAPHICS_API_VULKAN;
				else if (api == "dx12" || api == "directx12")
					gapi = GRAPHICS_API_DX12;
			}
		}
	}

	if (gapi == GRAPHICS_API_VULKAN)
		gDynamicRHI = new VulkanDynamicRHI();
	else
		gDynamicRHI = new DX12DynamicRHI();
	
	gGlobalPipeline = new GlobalPipeline();

	CORE_INFO("Current RHI: {}", gDynamicRHI->getName());

	gDynamicRHI->init();
	gDynamicRHI->createSwapchain(window);

	Renderer::init();

	AssetManager::init();
	PhysXWrapper::init();

	ImGuiWrapper::init(window);
}

enum DEMO
{
	DEMO_NONE,
	DEMO_TOWER,
	DEMO_OTHER,
};
static DEMO current_demo = DEMO_NONE;


void Application::run()
{
	double prev_time = glfwGetTime();
	float delta_seconds = 0.0f;
	

	if (current_demo != DEMO_NONE)
	{
		CubesDemo::initResources();
		tower_game.initResources();
		RenderTargetsDemo::initResources();
	}

	while (!glfwWindowShouldClose(window))
	{
		FrameMark;
		glfwPollEvents();

		gDynamicRHI->beginFrame();

		TransientResources::update();

		if (current_demo == DEMO_NONE)
		{
			ImGuiWrapper::begin();
			{
				PROFILE_CPU_SCOPE("Application::update");
				update(delta_seconds);
				updateBuffers(delta_seconds);
			}
		} else if (current_demo == DEMO_TOWER)
		{
			tower_game.update(delta_seconds);
		}

		// Record commands
		render(gDynamicRHI->getCmdList());

		gDynamicRHI->endFrame();

		if (framebuffer_resized)
		{
			framebuffer_resized = false;
			recreate_swapchain();
		}

		delta_seconds = glfwGetTime() - prev_time;
		prev_time = glfwGetTime();
	}

	cleanup();
}

void Application::render(RHICommandList *cmd_list)
{
	PROFILE_CPU_FUNCTION();
	PROFILE_GPU_FUNCTION(cmd_list);
	
	//vkCmdResetQueryPool(command_buffer.get_buffer(), VkWrapper::device->query_pools[Renderer::getCurrentFrameInFlight()], 0, VkWrapper::device->time_stamps[Renderer::getCurrentFrameInFlight()].size());
	
	//GPU_SCOPE_FUNCTION(&command_buffer);

	// Set swapchain color image layout for writing
	auto swapchain_texture = gDynamicRHI->getCurrentSwapchainTexture();
	swapchain_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
	//depth_stencil_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
	//cmd_list->setRenderTargets({swapchain_texture}, {depth_stencil_texture}, 0, 0, true);

	// Set PSO
	//cmd_list->setPipeline(pso_rhi);

	if (current_demo == DEMO_NONE)
	{
		recordCommands(cmd_list);
	} else if (current_demo == DEMO_TOWER)
	{
		tower_game.render();
	} else if (current_demo == DEMO_OTHER)
	{
		//auto swapchain_texture = gDynamicRHI->getCurrentSwapchainTexture();
		//swapchain_texture->transitLayout(cmd_list, TEXTURE_LAYOUT_ATTACHMENT);
		//cmd_list->setRenderTargets({swapchain_texture}, nullptr, 0, 0, true);

		//CubesDemo::render(cmd_list);
		//CubesDemo::renderBindless(cmd_list);
		//RenderTargetsDemo::render(cmd_list);
		//RenderTargetsDemo::renderFrameGraph(cmd_list);

		//ImGuiWrapper::render(cmd_list);
		//cmd_list->resetRenderTargets();
	}

	// Set swapchain color image layout for presenting
	gDynamicRHI->getCurrentSwapchainTexture()->transitLayout(cmd_list, TEXTURE_LAYOUT_PRESENT);
}

void Application::cleanup()
{
	Scene::closeScene();
	PhysXWrapper::shutdown();

	gDynamicRHI->waitGPU();

	AssetManager::shutdown();
	TransientResources::cleanup();
	ImGuiWrapper::shutdown();
	cleanupResources();
	///cleanup_swapchain();
	gDynamicRHI->getBindlessResources()->cleanup();
	// Shader::destroyAllShaders(); // TODO: implement
	Renderer::shutdown();

	delete gGlobalPipeline;
	gGlobalPipeline = nullptr;

	glfwDestroyWindow(window);
	glfwTerminate();

	gDynamicRHI->shutdown();
	delete gDynamicRHI;
	gDynamicRHI = nullptr;
}

void Application::cleanup_swapchain()
{
}

void Application::recreate_swapchain()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	gDynamicRHI->resizeSwapchain(width, height);
	Renderer::recreateScreenResources();
	onViewportSizeChanged(width, height);
}
