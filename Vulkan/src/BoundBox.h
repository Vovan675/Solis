#pragma once
#include <glm/glm.hpp>
#include "BoundFrustum.h"

struct BoundBox
{
	BoundBox() = default;
	BoundBox(glm::vec3 min, glm::vec3 max) : min(min), max(max) {}

	glm::vec3 getSize() const { return max - min; }
	glm::vec3 getCenter() const { return (max + min) / 2.0f; }

	void extend(const BoundBox& bound_box)
	{
		min.x = glm::min(min.x, bound_box.min.x);
		min.y = glm::min(min.y, bound_box.min.y);
		min.z = glm::min(min.z, bound_box.min.z);

		max.x = glm::min(max.x, bound_box.max.x);
		max.y = glm::min(max.y, bound_box.max.y);
		max.z = glm::min(max.z, bound_box.max.z);
	}

	void extend(const glm::vec3 &pos)
	{
		min.x = glm::min(min.x, pos.x);
		min.y = glm::min(min.y, pos.y);
		min.z = glm::min(min.z, pos.z);

		max.x = glm::max(max.x, pos.x);
		max.y = glm::max(max.y, pos.y);
		max.z = glm::max(max.z, pos.z);
	}

	bool isInside(BoundFrustum frustum);

	BoundBox operator *(const glm::mat4& mat);

	glm::vec3 min = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	glm::vec3 max = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
};