#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/quaternion.hpp"
#include "Application.h"
#include "Core/Variables.h"

class Camera
{
public:
	Camera()
	{
		updateMatrices();
	}

	Camera(glm::vec3 position) : position(position)
	{
		updateMatrices();
	}

	void update(float dt, glm::vec2 mouse_pos, bool look, bool pan, float mouse_wheel)
	{
		PROFILE_CPU_FUNCTION();
		dt = glm::min(dt, 0.1f);

		bool dragging = look || pan;
		glm::vec2 mouse_delta = dragging && was_dragging ? mouse_pos - prev_mouse_pos : glm::vec2(0, 0);
		prev_mouse_pos = mouse_pos;
		was_dragging = dragging;

		if (look)
		{
			pending_rotation += mouse_delta * 0.003f;
			speed = glm::clamp(speed * powf(1.2f, mouse_wheel), 0.05f, 1000.0f);
		}
		glm::vec2 rotation_step = glm::length(pending_rotation) < 0.001f ? pending_rotation : pending_rotation * (1.0f - expf(-dt * 30.0f));
		pending_rotation -= rotation_step;
		pitch = glm::clamp(pitch - rotation_step.y, glm::radians(-89.0f), glm::radians(89.0f));
		yaw -= rotation_step.x;
		updateMatrices();

		glm::vec3 forward = getForward();
		glm::vec3 right = getRight();
		glm::vec3 up = getUp();

		glm::vec3 target_velocity = glm::vec3(0, 0, 0);
		if (look)
		{
			if (gInput.isKeyDown(GLFW_KEY_W))
				target_velocity += forward;
			if (gInput.isKeyDown(GLFW_KEY_S))
				target_velocity -= forward;
			if (gInput.isKeyDown(GLFW_KEY_A))
				target_velocity -= right;
			if (gInput.isKeyDown(GLFW_KEY_D))
				target_velocity += right;
			if (gInput.isKeyDown(GLFW_KEY_E))
				target_velocity += up;
			if (gInput.isKeyDown(GLFW_KEY_Q))
				target_velocity -= up;
			if (target_velocity != glm::vec3(0, 0, 0))
				target_velocity = glm::normalize(target_velocity) * speed * (gInput.isKeyDown(GLFW_KEY_LEFT_SHIFT) ? 3.0f : 1.0f);
		}
		velocity += (target_velocity - velocity) * (1.0f - expf(-dt * 10.0f));
		if (target_velocity == glm::vec3(0, 0, 0) && glm::length(velocity) < speed * 0.05f)
			velocity = glm::vec3(0, 0, 0);
		if (!look)
			velocity += forward * mouse_wheel * speed;
		position += velocity * dt;

		if (pan)
			position += (up * mouse_delta.y - right * mouse_delta.x) * speed * 0.002f;

		if (mouse_delta != glm::vec2(0, 0) || velocity != glm::vec3(0, 0, 0) || pending_rotation != glm::vec2(0, 0))
			render_path_tracing_first_frame = true;
		updateMatrices();
	}

	void updateMatrices()
	{
		PROFILE_CPU_FUNCTION();
		orientation = glm::quat(glm::vec3(pitch, yaw, 0));
		view = glm::inverse(glm::translate(glm::mat4(1.0f), position) * glm::toMat4(orientation));
		proj = glm::perspective(glm::radians(fov), aspect, far_plane, near_plane); // Swapped intentionally, Inverse-Z
	}

	float getSpeed() const { return speed; }
	void setSpeed(float speed) { this->speed = speed; }

	float getFov() const { return fov; }
	void setFov(float fov) { this->fov = fov; updateMatrices(); }

	float getNear() const { return near_plane; }
	void setNear(float near_plane) { this->near_plane = near_plane; updateMatrices(); }

	float getFar() const { return far_plane; }
	void setFar(float far_plane) { this->far_plane = far_plane; updateMatrices(); }

	float getAspect() const { return aspect; }
	void setAspect(float aspect) { this->aspect = aspect; updateMatrices(); }

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
	glm::mat4 getViewProj() const { return proj * view; }

	glm::vec3 getForward() const { return glm::rotate(orientation, glm::vec3(0, 0, -1)); }
	glm::vec3 getRight() const { return glm::rotate(orientation, glm::vec3(1, 0, 0)); }
	glm::vec3 getUp() const { return glm::rotate(orientation, glm::vec3(0, 1, 0)); }

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
	glm::vec3 position = glm::vec3(0, 0, 0);
	glm::mat4 view;
	glm::mat4 proj;
	glm::quat orientation;
	float pitch = 0;
	float yaw = 0;
	float aspect = 1;

	float fov = 45.0f;

	glm::vec2 prev_mouse_pos;
	bool was_dragging = false;
	glm::vec2 pending_rotation = glm::vec2(0, 0);
	glm::vec3 velocity = glm::vec3(0, 0, 0);

	float near_plane = 0.1f;
	float far_plane = 10000.0f;
	float speed = 10.0f;

	friend struct Reflected<Camera>;
};

REFLECT_BEGIN(Camera)
	REFLECT_FIELD(position),
	REFLECT_FIELD(pitch),
	REFLECT_FIELD(yaw),
	REFLECT_FIELD(speed),
	REFLECT_FIELD(fov),
REFLECT_END()