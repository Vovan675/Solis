#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 inUV;

layout(location = 0) out float outAO;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=0) uniform UBO
{
	uint raw_tex_id;
} ubo;

void main() {

	vec2 texel_size = 1.0 / vec2(textureSize(textures[ubo.raw_tex_id], 0));

    int samples = 0;
    float result = 0.0;

    for (int x = -2; x <= 2; x++)
    {
        for (int y = -2; y <= 2; y++)
        {
            vec2 offset = vec2(float(x), float(y)) * texel_size;
            result += texture(textures[ubo.raw_tex_id], inUV + offset).r;
            samples++;
        }
    }

    outAO = result / float(samples);
}