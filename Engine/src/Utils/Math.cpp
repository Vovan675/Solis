#include "pch.h"
#include "Math.h"

glm::mat4 Math::getCubeFaceTransform(int face_index)
{
	eastl::vector<glm::mat4> views = {
		glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)), // X+
		glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0)), // X-
		glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, -1, 0), glm::vec3(0, 0, 1)), // Y+
		glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, -1)), // Y-
		glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0)), // Z+
		glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)), // Z-
	};
	return views[face_index];
}

uint32_t Math::alignedSize(uint32_t value, uint32_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

uint32_t Math::divideRoundUp(uint32_t nominator, uint32_t denominator)
{
	return (nominator + denominator - 1) / denominator;;
}

uint64_t Math::roundUp(uint64_t size, uint64_t granularity)
{
	const auto divUp = (size + granularity - 1) / granularity;
	return divUp * granularity;
}

bool Math::isPowerOfTwo(uint32_t x)
{
	return x && !(x & (x - 1));
}
