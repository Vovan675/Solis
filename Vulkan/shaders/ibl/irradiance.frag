layout (location=0) in vec3 dir;
layout (location=0) out vec4 outColor;

layout (binding=1) uniform samplerCube texSampler;

layout (set=1, binding=0) uniform sampler2D textures[];

#define PI 3.14159265359

void main()
{
	vec3 N = normalize(dir);

	vec3 up = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, N));
	up = normalize(cross(N, right));

	vec3 irradiance = vec3(0.0);
	int samples_count = 0;

	float step_phi = (2.0 * PI) / 180.0;
	float step_theta = (0.5 * PI) / 64.0;

	for(float phi = 0.0f; phi < 2.0 * PI; phi += step_phi)
	{
		for(float theta = 0.0f; theta < 0.5 * PI; theta += step_theta)
		{
			// Convert spherical coordinate to cartesian (local space, aka tangent space)
			vec3 sample_tangent = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
			vec3 sample_dir = sample_tangent.x * right + sample_tangent.y * up + sample_tangent.z * N;
			irradiance += texture(texSampler, sample_dir).rgb * cos(theta) * sin(theta);
			samples_count++;
		}
	}

	outColor = vec4(PI * irradiance / float(samples_count), 1.0);
}