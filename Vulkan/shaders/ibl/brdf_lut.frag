layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

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
    vec3 bitangent = normalize(cross(N, tangent));
	
    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}  

vec2 IntegrateBRDF(float NoV, float roughness)
{
    // Normal always points along z-axis for the 2D lookup 
	const vec3 N = vec3(0.0, 0.0, 1.0);

    vec3 V;
    V.x = sqrt(1.0 - NoV * NoV);
    V.y = 0.0f;
    V.z = NoV;
    
    const uint SAMPLES_COUNT = 1024u;

    vec2 sum = vec2(0, 0);
    for (uint i = 0u; i < SAMPLES_COUNT; i++)
    {
        vec2 Xi = Hammersley(i, SAMPLES_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            float NdotV = max(dot(N, V), 0.0);
            float NdotH = max(dot(N, H), 0.0);
            float VdotH = max(dot(V, H), 0.0);

            float G = G_SchlicksmithGGX(NdotL, NdotV, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            sum.x += (1.0 - Fc) * G_Vis;
            sum.y += Fc * G_Vis;
        }
    }

    sum.x /= float(SAMPLES_COUNT);
    sum.y /= float(SAMPLES_COUNT);
    return sum;
}

void main() {
    vec2 uv = inUV;
    outColor = vec4(IntegrateBRDF(uv.x, 1.0 - uv.y), 0, 1.0);
}