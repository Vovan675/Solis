#include "pch.h"
#include "Math.h"

glm::mat4 Math::getCubeFaceTransform(int face_index)
{
	static std::vector<glm::mat4> views = {
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
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
