#pragma once
#include "GLFW/glfw3.h"

class Input
{
public:
	void init(GLFWwindow *window)
	{
		this->window = window;
	}

	bool isKeyDown(int key)
	{
		return glfwGetKey(window, key);
	}

	bool isMouseKeyDown(int button)
	{
		return glfwGetMouseButton(window, button) == GLFW_PRESS;
	}

	glm::vec2 getMousePosition()
	{
		double mouse_x, mouse_y;
		glfwGetCursorPos(window, &mouse_x, &mouse_y);
		return glm::vec2(mouse_x, mouse_y);
	}

private:
	GLFWwindow *window;
};