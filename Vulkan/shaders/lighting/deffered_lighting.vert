#include "common.h"

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inNormal;

layout (binding = 0) uniform UBO 
{
	mat4 model;
	mat4 light_matrix;
} ubo;

layout (location = 0) out vec4 outPos;

void main() 
{
	gl_Position = projection * view * ubo.model * inPos;
	outPos = projection * view * ubo.model * inPos;
}
