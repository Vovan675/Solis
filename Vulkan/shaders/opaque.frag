layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inColor;
layout(location = 4) in mat3 inTBN;

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
	int normal_tex_id;
	int padding[3];
};

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outShading;

void main()
{
	outColor = albedo_tex_id >= 0 ? texture(textures[albedo_tex_id], inUV) : albedo;
	if (outColor.a < 1.0)
		discard;

	outNormal = vec4(normalize(inNormal), 1);

	if (normal_tex_id >= 0)
	{
		vec3 normal = texture(textures[normal_tex_id], inUV).rgb;
		normal = normalize(normal * 2.0 - 1.0);
		outNormal.rgb = normalize(inTBN * -normal);
	}

	#if 0
		outShading.r = metalness_tex_id >= 0 ? texture(textures[metalness_tex_id], inUV).r : shading.r;
		outShading.g = roughness_tex_id >= 0 ? texture(textures[roughness_tex_id], inUV).r : shading.g;
		outShading.b = specular_tex_id >= 0 ? texture(textures[specular_tex_id], inUV).r : shading.b;
	#else // Bistro specular map format
		outShading.r = specular_tex_id >= 0 ? texture(textures[specular_tex_id], inUV).b : shading.r;
		outShading.g = specular_tex_id >= 0 ? texture(textures[specular_tex_id], inUV).g : shading.g;
		outShading.b = shading.b;
	#endif
	outShading.a = 1.0;
}