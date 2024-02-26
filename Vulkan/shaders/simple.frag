#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragColor;

layout(binding=1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

const vec3 LIGHT_POS = vec3(10, 0, 0);

void main()
{
	// Lighting
	vec3 lightDir = normalize(LIGHT_POS - fragPos);
	float ambient = 0.2;
	float diffuse = clamp(dot(lightDir, fragNormal), 0.0, 1.0);
	vec4 light = vec4(fragColor * (ambient + diffuse), 1.0);
	outColor = vec4(fragUV.xy, 0.0, 1.0);
	outColor = texture(texSampler, fragUV) * light;
}