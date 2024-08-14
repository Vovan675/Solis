layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inColor;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(push_constant) uniform constants
{
	mat4 model;
	vec4 albedo;
	int albedo_tex_id;
	int metalness_tex_id;
	int roughness_tex_id;
	int specular_tex_id;
	vec4 shading;
};

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 3) out vec4 outShading;

void main()
{
	outColor = albedo_tex_id >= 0 ? texture(textures[albedo_tex_id], inUV) : albedo;
	outNormal = vec4(normalize(inNormal), 1);

	outShading.r = metalness_tex_id >= 0 ? texture(textures[metalness_tex_id], inUV).r : shading.r;
	outShading.g = roughness_tex_id >= 0 ? texture(textures[roughness_tex_id], inUV).r : shading.g;
	outShading.b = specular_tex_id >= 0 ? texture(textures[specular_tex_id], inUV).r : shading.b;
	outShading.a = 1.0;
}