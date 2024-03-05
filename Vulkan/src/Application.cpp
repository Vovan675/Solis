#include "pch.h"
#include "Application.h"
#include "VkWrapper.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "imgui.h"

Application::Application()
{
	camera = std::make_shared<Camera>();

	// Load image
	TextureDescription tex_description;
	tex_description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	auto texture = std::make_shared<Texture>(tex_description);
	texture->load("assets/albedo2.png");
	auto texture2 = std::make_shared<Texture>(tex_description);
	texture2->load("assets/albedo.png");

	// Load mesh
	auto mesh = std::make_shared<Engine::Mesh>("assets/model2.obj");
	auto mesh2 = std::make_shared<Engine::Mesh>("assets/model.fbx");

	//cubemap_renderer = std::make_shared<CubeMapRenderer>(camera);
	mesh_renderer = std::make_shared<MeshRenderer>(camera, mesh, texture);
	mesh_renderer2 = std::make_shared<MeshRenderer>(camera, mesh2, texture2);
	imgui_renderer = std::make_shared<ImGuiRenderer>(window);
}

void Application::update(float delta_time)
{
	double mouse_x, mouse_y;
	glfwGetCursorPos(window, &mouse_x, &mouse_y);
	camera->update(delta_time, glm::vec2(mouse_x, mouse_y), glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS);

	imgui_renderer->begin();

	// Draw Imgui Windows
	ImGui::ShowDemoWindow();
}

void Application::updateBuffers(float delta_time, uint32_t image_index)
{
	mesh_renderer->setPosition(glm::vec3(0, 0, 0));
	mesh_renderer->updateUniformBuffer(image_index);

	mesh_renderer2->setPosition(glm::vec3(3, 0, 0));
	mesh_renderer2->setScale(glm::vec3(0.2f, 0.2f, 0.2f));
	mesh_renderer2->updateUniformBuffer(image_index);
	//cubemap_renderer->updateUniformBuffer(image_index);
}

void Application::recordCommands(CommandBuffer & command_buffer, uint32_t image_index)
{
	// Render cubemap
	//cubemap_renderer->fillCommandBuffer(command_buffer, image_index);

	// Render mesh
	mesh_renderer->fillCommandBuffer(command_buffer, image_index);
	mesh_renderer2->fillCommandBuffer(command_buffer, image_index);

	// Render imgui
	imgui_renderer->fillCommandBuffer(command_buffer, image_index);
}

void Application::cleanupResources()
{
	mesh_renderer = nullptr;
	mesh_renderer2 = nullptr;
	imgui_renderer = nullptr;
}

void Application::key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	bool is_pressed = action != GLFW_RELEASE;
	if (key == GLFW_KEY_W)
		camera->inputs.forward = is_pressed;
	if (key == GLFW_KEY_S)
		camera->inputs.backward = is_pressed;
	if (key == GLFW_KEY_A)
		camera->inputs.left = is_pressed;
	if (key == GLFW_KEY_D)
		camera->inputs.right = is_pressed;
}

