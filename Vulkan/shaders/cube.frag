#version 450

layout (location=0) in vec3 dir;
layout (location=0) out vec4 outColor;

layout (binding=1) uniform samplerCube texSampler;

layout (set=1, binding=0) uniform sampler2D textures[];

void main()
{
	outColor = textureLod(texSampler, dir, 0.0);
	//outColor = vec4(1, 0,0,1);
}