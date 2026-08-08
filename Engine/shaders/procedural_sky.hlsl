#include "common.h"

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
};

struct VertexOutput {
    float4 position : SV_POSITION;
    float3 dir : TEXCOORD0;
};

cbuffer UBO : register(b0)
{
    float3 sunPosition;
    float4x4 mvp;
    float sky_luminance_scale;
};

VertexOutput VSMain(VertexInput IN) {
    VertexOutput OUT;
    OUT.position = mul(mvp, float4(IN.position, 1.0));
    OUT.dir = normalize(IN.position);
    return OUT;
}

static const float depolarizationFactor = 0.067;
static const float mieCoefficient = 0.00335;
static const float mieDirectionalG = 0.787;
static const float3 mieKCoefficient = float3(0.686, 0.678, 0.666);
static const float mieV = 4.012;
static const float mieZenithLength = 500;
static const float numMolecules = 2.542e25;
static const float3 primaries = float3(6.8e-7, 5.5e-7, 4.5e-7);
static const float rayleigh = 1.0;
static const float rayleighZenithLength = 615;
static const float refractiveIndex = 1.000317;
static const float sunIntensityFactor = 1111;
static const float sunIntensityFalloffSteepness = 0.98;
static const float turbidity = 1.25;

static const float3 cameraPos = float3(100000.0, -40000.0, 0.0);
static const float3 UP = float3(0.0, 1.0, 0.0);

float3 totalRayleigh(float3 lambda)
{
    return (8.0 * pow(PI, 3.0) * pow(pow(refractiveIndex, 2.0) - 1.0, 2.0) * (6.0 + 3.0 * depolarizationFactor)) / (3.0 * numMolecules * pow(lambda, 4.0) * (6.0 - 7.0 * depolarizationFactor));
}

float3 totalMie(float3 lambda, float3 K, float T)
{
    float c = 0.2 * T * 10e-18;
    return 0.434 * c * PI * pow((2.0 * PI) / lambda, mieV - 2.0) * K;
}

float rayleighPhase(float cosTheta)
{
    return (3.0 / (16.0 * PI)) * (1.0 + pow(cosTheta, 2.0));
}

float henyeyGreensteinPhase(float cosTheta, float g)
{
    return (1.0 / (4.0 * PI)) * ((1.0 - pow(g, 2.0)) / pow(1.0 - 2.0 * g * cosTheta + pow(g, 2.0), 1.5));
}

float sunIntensity(float zenithAngleCos)
{
    float cutoffAngle = PI / 1.95;
    return sunIntensityFactor * max(0.0, 1.0 - exp(-((cutoffAngle - acos(zenithAngleCos)) / sunIntensityFalloffSteepness)));
}

float4 PSMain(VertexOutput IN) : SV_TARGET
{
	// Rayleigh coefficient
    float sunfade = 1.0 - clamp(1.0 - exp((sunPosition.y / 450000.0)), 0.0, 1.0);
    float rayleighCoefficient = rayleigh - (1.0 * (1.0 - sunfade));
    float3 betaR = totalRayleigh(primaries) * rayleighCoefficient;
    
	// Mie coefficient
    float3 betaM = totalMie(primaries, mieKCoefficient, turbidity) * mieCoefficient;
    
	// Optical length, cutoff angle at 90 to avoid singularity
    float zenithAngle = acos(max(0.0, dot(UP, normalize(IN.dir))));
    float denom = cos(zenithAngle) + 0.15 * pow(93.885 - ((zenithAngle * 180.0) / PI), -1.253);
    float sR = rayleighZenithLength / denom;
    float sM = mieZenithLength / denom;
    
	// Combined extinction factor
    float3 Fex = exp(-(betaR * sR + betaM * sM));
    
	// In-scattering
    float3 sunDirection = normalize(sunPosition);
    float cosTheta = dot(normalize(IN.dir), sunDirection);
    float3 betaRTheta = betaR * rayleighPhase(cosTheta * 0.5 + 0.5);
    float3 betaMTheta = betaM * henyeyGreensteinPhase(cosTheta, mieDirectionalG);
    float sunE = sunIntensity(dot(sunDirection, UP));
    float3 Lin = pow(sunE * ((betaRTheta + betaMTheta) / (betaR + betaM)) * (1.0 - Fex), 1.5);
    Lin *= lerp(1.0, pow(sunE * ((betaRTheta + betaMTheta) / (betaR + betaM)) * Fex, 0.5), clamp(pow(1.0 - dot(UP, sunDirection), 5.0), 0.0, 1.0));
    
    return float4((Lin + 0.1 * Fex) * sky_luminance_scale, 1);
}