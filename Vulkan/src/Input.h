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

private:
	GLFWwindow *window;
};