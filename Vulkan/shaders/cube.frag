layout (location=0) in vec3 dir;
layout (location=0) out vec4 outColor;

layout (binding=1) uniform samplerCube texSampler;

layout (set=1, binding=0) uniform sampler2D textures[];

void main()
{
	outColor = vec4(textureLod(texSampler, dir, 0.0).rgb, 1.0);
}