#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=0) uniform UBO
{
	uint present_mode;
	uint albedo_tex_id;
	uint normal_tex_id;
} ubo;

void main() {
    uint mode = ubo.present_mode;

    vec2 uv = inUV;
    vec4 value = vec4(1, 1, 1, 1);
    const float border = 0.001;

    if (ubo.present_mode == 0)
    {
        if (uv.x > 1.0 / 2.0)
        {
            mode = 1;
        } else if (uv.x < 1.0 / 2.0 - border)
        {
            mode = 2;
        }
        uv.x *= 2.0;
    }
    
    vec4 albedo = texture(textures[ubo.albedo_tex_id], uv);
    vec4 normal = texture(textures[ubo.normal_tex_id], uv);

    if (mode == 1)
    {
        value = albedo;
    } else if (mode == 2)
    {   
        value = normal;
    }
    outColor = vec4(value.rgb, 1.0);
}