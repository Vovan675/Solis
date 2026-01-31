#pragma once
#include <glm/glm.hpp>
#include "BoundBox.h"
#include "BoundFrustum.h"

struct BoundSphere
{
	BoundSphere() = default;
	BoundSphere(glm::vec3 center, float radius) : center(center), radius(radius) {}
	explicit BoundSphere(BoundBox bound_box)
	{
		center = bound_box.getCenter();
		radius = glm::length(center - bound_box.max);
	}

	bool isInside(BoundFrustum frustum) const
	{
		for (const glm::vec4& plane : frustum.planes)
		{
			glm::vec3 normal(plane.x, plane.y, plane.z);

			float distance = glm::dot(normal, center) + plane.w;
			if (distance < -radius)
				return false;
		}
		return true;
	}

	BoundSphere operator*(const glm::mat4 &mat)
	{
		glm::vec3 center_new = glm::vec3(mat * glm::vec4(center, 1.0f));

		float scale_x = glm::length2(glm::vec3(mat[0]));
		float scale_y = glm::length2(glm::vec3(mat[1]));
		float scale_z = glm::length2(glm::vec3(mat[2]));

		float scale = glm::max(scale_x, scale_y, scale_z);

		float radius_new = radius * sqrtf(scale);
		return BoundSphere(center_new, radius_new);
	}
	
	glm::vec3 center = glm::vec3(0);
	float radius = 0.0f;
};