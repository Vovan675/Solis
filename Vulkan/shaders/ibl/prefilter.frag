#version 450
layout(location = 0) in vec3 dir;
layout(location = 0) out vec4 outColor;

layout (binding=1) uniform samplerCube texSampler;

layout(push_constant) uniform constants
{
	layout(offset = 64) float roughness;
} PushConstants;

const float PI = 3.14159265359;

float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}

float G_SchlicksmithGGX(float NdotL, float NdotV, float roughness)
{
	float k = (roughness * roughness) / 2.0;
	float GL = NdotL / (NdotL * (1.0 - k) + k);
	float GV = NdotV / (NdotV * (1.0 - k) + k);
	return GL * GV;
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness*roughness;
	
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
    // from spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
	
    // from tangent-space vector to world-space sample vector
    vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
	
    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}  

vec3 prefilterEnvMap(vec3 R, float roughness)
{
    // Normal always points along z-axis for the 2D lookup 
	const vec3 N = normalize(R);
    vec3 V = R;

    const uint SAMPLES_COUNT = 1024u;

    float total_weight = 0.0;
    vec3 prefiltered_color = vec3(0, 0, 0);
    for (uint i = 0u; i < SAMPLES_COUNT; i++)
    {
        vec2 Xi = Hammersley(i, SAMPLES_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = clamp(dot(N, L), 0.0, 1.0);
        if (NdotL > 0.0)
        {
            //float NdotV = max(dot(N, V), 0.0);
            //float NdotH = max(dot(N, H), 0.0);
            //float VdotH = max(dot(V, H), 0.0);

            prefiltered_color += texture(texSampler, L).rgb * NdotL;
            total_weight += NdotL;
        }
    }

    prefiltered_color /= total_weight;
    return prefiltered_color;
}

void main() {
    vec3 N = normalize(dir);
    outColor = vec4(prefilterEnvMap(N, PushConstants.roughness), 1.0);
}