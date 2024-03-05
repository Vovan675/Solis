#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/quaternion.hpp"

class Camera
{
public:
	Camera() 
	{
		position = glm::vec3(2, -2, 4);
		pitch = 0;
		yaw = 0;
		updateMatrices();
	}

	void update(float dt, glm::vec2 mouse_pos, bool mouse_pressed)
	{
		if (mouse_pressed)
		{
			glm::vec2 delta_pos = mouse_pos - prev_mouse_pos;
			delta_pos *= 0.003f;
			pitch += delta_pos.y;
			yaw += -delta_pos.x;
			updateMatrices();
		}
		prev_mouse_pos = mouse_pos;

		// Get rows for directions
		glm::vec3 forward = glm::rotate(orientation, glm::vec3(0, 0, 1));
		glm::vec3 right = glm::rotate(orientation, glm::vec3(-1, 0, 0));
		glm::vec3 up = glm::rotate(orientation, glm::vec3(0, 1, 0));

		glm::vec3 movement = glm::vec3(0, 0, 0);
		if (inputs.forward)
			movement += forward;
		if (inputs.backward)
			movement -= forward;
		if (inputs.left)
			movement -= right;
		if (inputs.right)
			movement += right;

		movement *= dt * 2.0;
		position += movement;
		updateMatrices();
	}

	void updateMatrices()
	{
		orientation = glm::quat(glm::vec3(pitch, yaw, 0));
		view = glm::inverse(glm::translate(glm::mat4(1.0f), position) * glm::toMat4(orientation));
	}

	const glm::mat4& getView() const { return view; }

	struct Inputs
	{
		bool forward = false;
		bool backward = false;
		bool left = false;
		bool right = false;
	} inputs;

private:
	glm::vec3 position;
	glm::mat4 view;
	glm::quat orientation;
	float pitch, yaw;

	glm::vec2 prev_mouse_pos;
};