#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inColor;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=1) uniform MaterialUBO
{
	uint albedoTexId;
	uint rougnessTexId;
	float useRoughnessMap;
} material;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;

void main()
{
	outColor = texture(textures[material.albedoTexId], inUV);
	outNormal = vec4(normalize(inNormal), 1);
}