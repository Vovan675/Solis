#version 450

layout (location=0) in vec3 dir;
layout (location=0) out vec4 outColor;

layout (binding=1) uniform samplerCube texSampler;

void main()
{
	outColor = texture(texSampler, dir);
};