#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/quaternion.hpp"
#include "Application.h"

class Camera
{
public:
	Camera() 
	{
		position = glm::vec3(0, 2, 0);
		pitch = 0;
		yaw = 0;
		updateMatrices();
	}

	void update(float dt, glm::vec2 mouse_pos, bool mouse_pressed)
	{
		PROFILE_CPU_FUNCTION();
		dt = 1.0f / 60.0f;
		if (mouse_pressed)
		{
			glm::vec2 delta_pos = mouse_pos - prev_mouse_pos;
			delta_pos *= 0.003f;
			pitch += delta_pos.y;
			yaw += delta_pos.x;
			updateMatrices();
		}
		prev_mouse_pos = mouse_pos;

		// Get rows for directions
		glm::vec3 forward = glm::rotate(orientation, glm::vec3(0, 0, 1));
		glm::vec3 right = glm::rotate(orientation, glm::vec3(1, 0, 0));
		glm::vec3 up = glm::rotate(orientation, glm::vec3(0, 1, 0));

		glm::vec3 movement = glm::vec3(0, 0, 0);
		if (gInput.isKeyDown(GLFW_KEY_W))
			movement += forward;
		if (gInput.isKeyDown(GLFW_KEY_S))
			movement -= forward;
		if (gInput.isKeyDown(GLFW_KEY_A))
			movement -= right;
		if (gInput.isKeyDown(GLFW_KEY_D))
			movement += right;
		if (gInput.isKeyDown(GLFW_KEY_E))
			movement += up;
		if (gInput.isKeyDown(GLFW_KEY_Q))
			movement -= up;
		
		if (gInput.isKeyDown(GLFW_KEY_LEFT_SHIFT))
			movement *= 2.0f;

		movement *= dt * speed;
		position += movement;
		updateMatrices();
	}

	void updateMatrices()
	{
		PROFILE_CPU_FUNCTION();
		orientation = glm::quat(glm::vec3(pitch, yaw, 0));
		view = glm::inverse(glm::translate(glm::mat4(1.0f), position) * glm::toMat4(orientation));
		proj = glm::perspectiveLH(glm::radians(45.0f), aspect, near_plane, far_plane);
	}

	void setSpeed(float speed) { this->speed = speed; }
	float getSpeed() const { return speed; }

	void setNear(float near_plane) { this->near_plane = near_plane; }
	float getNear() const { return near_plane; }

	void setFar(float far_plane) { this->far_plane = far_plane; }
	float getFar() const { return far_plane; }

	void setAspect(float aspect) { this->aspect = aspect; }
	float getAspect() const { return aspect; }

	const glm::vec3 &getPosition() const { return position; }
	void setPosition(glm::vec3 position) { this->position = position; updateMatrices(); }

	const glm::vec3 &getRotation() const { return glm::vec3(pitch, yaw, 0); }
	void setRotation(glm::vec3 rotation)
	{  
		pitch = glm::radians(rotation.x);
		yaw = glm::radians(rotation.y);
		updateMatrices();
	}

	const glm::mat4 &getView() const { return view; }
	const glm::mat4 &getProj() const { return proj; }

	struct Inputs
	{
		bool forward = false;
		bool backward = false;
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
		bool sprint = false;
	} inputs;

private:
	glm::vec3 position;
	glm::mat4 view;
	glm::mat4 proj;
	glm::quat orientation;
	float pitch, yaw;
	float aspect;

	glm::vec2 prev_mouse_pos;

	float near_plane = 0.1f;
	float far_plane = 200.0f;
	float speed = 2.0f;
};