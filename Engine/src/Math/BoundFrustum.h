#pragma once
#include <glm/glm.hpp>

struct BoundFrustum
{
	BoundFrustum() = default;
	BoundFrustum(glm::mat4 proj, glm::mat4 view)
	{
		// Left
		planes[0] = glm::vec4(
			proj[0][3] + proj[0][0], // A
			proj[1][3] + proj[1][0], // B
			proj[2][3] + proj[2][0], // C
			proj[3][3] + proj[3][0]  // D
		);

		// Right
		planes[1] = glm::vec4(
			proj[0][3] - proj[0][0],
			proj[1][3] - proj[1][0],
			proj[2][3] - proj[2][0],
			proj[3][3] - proj[3][0]
		);

		// Bottom
		planes[2] = glm::vec4(
			proj[0][3] + proj[0][1],
			proj[1][3] + proj[1][1],
			proj[2][3] + proj[2][1],
			proj[3][3] + proj[3][1]
		);

		// Top
		planes[3] = glm::vec4(
			proj[0][3] - proj[0][1],
			proj[1][3] - proj[1][1],
			proj[2][3] - proj[2][1],
			proj[3][3] - proj[3][1]
		);

		// Near
		planes[4] = glm::vec4(
			proj[0][3] + proj[0][2],
			proj[1][3] + proj[1][2],
			proj[2][3] + proj[2][2],
			proj[3][3] + proj[3][2]
		);

		// Far
		planes[5] = glm::vec4(
			proj[0][3] - proj[0][2],
			proj[1][3] - proj[1][2],
			proj[2][3] - proj[2][2],
			proj[3][3] - proj[3][2]
		);

		// Normalize planes
		for (auto& plane : planes)
		{
			plane = plane * view;
			float length = glm::length(glm::vec3(plane));
			plane /= length;
		}
	}

	glm::vec4 planes[6];
};