#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=0) uniform UBO
{
    uint lighting_diffuse_tex_id;
	uint lighting_specular_tex_id;
	uint albedo_tex_id;
	uint normal_tex_id;
	uint depth_tex_id;
} ubo;

layout (set=0, binding=1) uniform samplerCube irradiance_tex;


void main() {
    float depth = texture(textures[ubo.depth_tex_id], inUV).r;

    if (depth == 1.0)
        discard;

    vec4 albedo = texture(textures[ubo.albedo_tex_id], inUV);
    vec3 normal = normalize(texture(textures[ubo.normal_tex_id], inUV).rgb);

    vec3 light_diffuse = texture(textures[ubo.lighting_diffuse_tex_id], inUV).rgb;
    vec3 light_specular = texture(textures[ubo.lighting_specular_tex_id], inUV).rgb;

    // IBL
    vec3 irradiance = texture(irradiance_tex, normal).rgb;
    vec3 ibl_diffuse = irradiance * albedo.rgb;

    vec3 ambient = ibl_diffuse * 0.3f;

    outColor = vec4(albedo.rgb * light_diffuse + light_specular + ambient, albedo.a);
}