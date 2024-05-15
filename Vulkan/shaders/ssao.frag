#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 inUV;

layout(location = 0) out float outAO;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=0) uniform UBO
{
	uint normal_tex_id;
	uint depth_tex_id;
    uint position_tex_id;
    vec3 screen_size;
    vec4 kernel[64];
    mat4 view;
    mat4 proj;
    float near;
    float far;
    int samples;
    float sample_radius;
} ubo;

layout (set=0, binding=1) uniform sampler2D noise_tex;


vec3 getVSPosition(vec2 uv, float hardware_depth)
{
    vec4 pos = inverse(ubo.proj) * vec4(uv * 2 - 1, hardware_depth, 1.0f);
    pos /= pos.w;
    return pos.xyz;
}

void main() {
    float depth = texture(textures[ubo.depth_tex_id], inUV).r;

    //if (depth == 1.0)
    //    discard;

    vec2 noise_scale = ubo.screen_size.xy / 4.0f;

    vec3 normal = normalize(texture(textures[ubo.normal_tex_id], inUV).rgb);
    normal = normalize(transpose(inverse(mat3(ubo.view))) * normal); // world -> view space

    vec3 view_pos = getVSPosition(inUV, depth);

	ivec2 tex_dim = textureSize(textures[ubo.depth_tex_id], 0); 
	ivec2 noise_dim = textureSize(noise_tex, 0);
	const vec2 noise_uv = vec2(float(tex_dim.x)/float(noise_dim.x), float(tex_dim.y)/(noise_dim.y)) * inUV;  
    vec3 noise = texture(noise_tex, noise_uv).rgb * 2 - 1;

    vec3 tangent = normalize(noise - normal * dot(noise, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0f;
    for (int i = 0; i < ubo.samples; i++)
    {
        vec3 sample_pos = TBN * ubo.kernel[i].xyz;
        sample_pos = view_pos + sample_pos * ubo.sample_radius;

        vec4 offset = vec4(sample_pos, 1.0);
        offset = ubo.proj * offset; // view -> clip space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // [-1, 1] -> [0, 1]

        float sample_depth = texture(textures[ubo.depth_tex_id], offset.xy).r;
        vec3 sample_view_pos = getVSPosition(offset.xy, sample_depth);

        float range_check = smoothstep(0.0, 1.0, ubo.sample_radius / abs(view_pos.z - sample_view_pos.z));
        occlusion += (sample_view_pos.z >= sample_pos.z + 0.025 ? 1.0 : 0.0) * range_check;
    }

    occlusion = 1.0 - (occlusion / ubo.samples);
    outAO = occlusion;
}