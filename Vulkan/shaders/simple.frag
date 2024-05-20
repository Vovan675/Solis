layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragColor;

layout (set=1, binding=0) uniform sampler2D textures[];


layout(set=0, binding=0) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
	vec3 cameraPosition;
} ubo;

layout(set=0, binding=1) uniform MaterialUBO
{
	uint albedoTexId;
	uint rougnessTexId;
	float useRoughnessMap;
} material;

layout(location = 0) out vec4 outColor;

// CONSTS
const vec3 LIGHT_POS = vec3(1, 1, 0);
const float PI = 3.14159265358;

void main()
{
	// Lighting
	vec3 N = normalize(fragNormal);
	vec3 L = normalize(LIGHT_POS - fragPos);
	vec3 V = normalize(ubo.cameraPosition - fragPos);
	vec3 R = reflect(L, N);

	float ambient = 0.2;
	float diffuse = clamp(dot(N, L), 0.0, 1.0);
	float specular = pow(max(dot(L, V), 0.0), 16.0);
	specular = 0;

	vec4 light = vec4(fragColor * (ambient + diffuse) + vec3(1.0) * specular, 1.0);
	outColor = vec4(fragUV.xy, 0.0, 1.0);
	outColor = texture(textures[0], fragUV);
	if (material.useRoughnessMap > 0)
		outColor = vec4(fragUV.xy, 0.0, 1.0);
}