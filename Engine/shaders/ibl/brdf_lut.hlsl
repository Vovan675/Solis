// Constants
static const float PI = 3.14159265359;

// Input/Output
struct PSInput {
    float2 uv : TEXCOORD0;
};

struct PSOutput {
    float4 color : SV_Target0;
};

// Radical Inverse Van der Corput
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Hammersley Sequence
float2 Hammersley(uint i, uint N) {
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

// Schlick-Smith GGX Geometry Function
float G_SchlicksmithGGX(float NdotL, float NdotV, float roughness) {
    float k = (roughness * roughness) / 2.0;
    float GL = NdotL / (NdotL * (1.0 - k) + k);
    float GV = NdotV / (NdotV * (1.0 - k) + k);
    return GL * GV;
}

// Importance Sampling for GGX NDF
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness) {
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // Spherical to Cartesian coordinates
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // Tangent space to world space
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = normalize(cross(N, tangent));

    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

// BRDF Integration
float2 IntegrateBRDF(float NoV, float roughness) {
    // Normal always points along z-axis for the 2D lookup
    const float3 N = float3(0.0, 0.0, 1.0);

    float3 V;
    V.x = sqrt(1.0 - NoV * NoV);
    V.y = 0.0;
    V.z = NoV;

    const uint SAMPLES_COUNT = 1024u;

    float2 sum = float2(0, 0);
    for (uint i = 0u; i < SAMPLES_COUNT; i++) {
        float2 Xi = Hammersley(i, SAMPLES_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
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

PSOutput PSMain(PSInput input) {
    PSOutput output;
    float2 uv = input.uv;
    output.color = float4(IntegrateBRDF(uv.x, 1.0 - uv.y), 0.0, 1.0);
    return output;
}