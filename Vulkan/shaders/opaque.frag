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
	vec4 albedo;
	float useAlbedoMap;

	uint rougnessTexId;
	float useRoughnessMap;
	float metalness;
	float roughness;
	float specular;
} material;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec3 outPosition;
layout(location = 3) out vec4 outShading;

void main()
{
	if (material.useAlbedoMap > 0.5)
		outColor = texture(textures[material.albedoTexId], inUV);
	else
		outColor = material.albedo;
	outNormal = vec4(normalize(inNormal), 1);
	outPosition = inPos;
	outShading = vec4(material.metalness, material.roughness, material.specular, 1.0f);
}