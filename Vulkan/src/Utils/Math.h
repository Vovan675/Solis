#pragma once

namespace Math
{
	glm::mat4 getCubeFaceTransform(int face_index);

	uint32_t alignedSize(uint32_t value, uint32_t alignment);

	uint64_t roundUp(uint64_t size, uint64_t granularity);
}