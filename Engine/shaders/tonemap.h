#pragma once
#include "bindless.h"

#define TONEMAP_LUT_SIZE 32

static const float MIDDLE_GREY = 0.18;
static const float TONEMAP_LUT_STOPS = 20.0;

float3 linearToLog(float3 linear_color)
{
	return saturate(log2(linear_color / MIDDLE_GREY) / TONEMAP_LUT_STOPS + 0.5);
}

float3 logToLinear(float3 log_color)
{
	return MIDDLE_GREY * exp2((log_color - 0.5) * TONEMAP_LUT_STOPS);
}

float3 sampleTonemapLut(uint lut_tex_id, float3 linear_color)
{
	Texture3D lut = ResourceDescriptorHeap[lut_tex_id];
	float3 texel = linearToLog(linear_color) * (TONEMAP_LUT_SIZE - 1.0);
	float3 uvw = (texel + 0.5) / TONEMAP_LUT_SIZE;
	return lut.SampleLevel(linear_clamp_sampler, uvw, 0).rgb;
}

// Color spaces
static const float3x3 REC709_TO_XYZ =
{
	{ 0.4123907993, 0.3575843394, 0.1804807884 },
	{ 0.2126390059, 0.7151686788, 0.0721923154 },
	{ 0.0193308187, 0.1191947798, 0.9505321522 }
};

static const float3x3 XYZ_TO_REC709 =
{
	{ 3.2409699419, -1.5373831776, -0.4986107603 },
	{ -0.9692436363, 1.8759675015, 0.0415550574 },
	{ 0.0556300797, -0.2039769589, 1.0569715142 }
};

static const float3x3 AP1_TO_XYZ =
{
	{ 0.6624541811, 0.1340042065, 0.1561876870 },
	{ 0.2722287168, 0.6740817658, 0.0536895174 },
	{ -0.0055746495, 0.0040607335, 1.0103391003 }
};

static const float3x3 XYZ_TO_AP1 =
{
	{ 1.6410233797, -0.3248032942, -0.2364246952 },
	{ -0.6636628587, 1.6153315917, 0.0167563477 },
	{ 0.0117218943, -0.0082844420, 0.9883948585 }
};

static const float3x3 D65_TO_D60 =
{
	{ 1.0130349146, 0.0061052578, -0.0149709436 },
	{ 0.0076982301, 0.9981633521, -0.0050320385 },
	{ -0.0028413174, 0.0046851567, 0.9245061375 }
};

static const float3x3 D60_TO_D65 =
{
	{ 0.9872240087, -0.0061132286, 0.0159532883 },
	{ -0.0075983718, 1.0018614847, 0.0053300358 },
	{ 0.0030725771, -0.0050959615, 1.0816806031 }
};

static const float3x3 REC709_TO_AP1 = mul(XYZ_TO_AP1, mul(D65_TO_D60, REC709_TO_XYZ));
static const float3x3 AP1_TO_REC709 = mul(XYZ_TO_REC709, mul(D60_TO_D65, AP1_TO_XYZ));

// Bradford LMS (standartly used, because this LMS adjusted to human perception)
// Matrix Reference: http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html
static const float3x3 XYZ_TO_BRADFORD_LMS =
{
	{ 0.8951, 0.2664, -0.1614 },
	{ -0.7502, 1.7135, 0.0367 },
	{ 0.0389, -0.0685, 1.0296 }
};

static const float3x3 BRADFORD_LMS_TO_XYZ =
{
	{ 0.9869929, -0.1470543, 0.1599627 },
	{ 0.4323053, 0.5183603, 0.0492912 },
	{ -0.0085287, 0.0400428, 0.9684867 }
};

// CIE 1960 uv to CIE 1931 xy, https://en.wikipedia.org/wiki/CIE_1960_color_space#Relation_to_CIE_XYZ
float2 uvToXY(float2 uv)
{
	return float2(3.0 * uv.x, 2.0 * uv.y) / (2.0 * uv.x - 8.0 * uv.y + 4.0);
}

// xy to XYZ with Y = 1, https://en.wikipedia.org/wiki/CIE_1931_color_space#CIE_xyY_color_space
float3 xyToXYZ(float2 xy)
{
	return float3(xy.x / xy.y, 1.0, (1.0 - xy.x - xy.y) / xy.y);
}

// Planck's locus approximation in CIE 1960 color space, 1000K-15000K,
// Reference: https://en.wikipedia.org/wiki/Planckian_locus#Approximation
float2 planckianLocusUV(float temperature)
{
	float t = temperature;
	float u = (0.860117757 + 1.54118254e-4 * t + 1.28641212e-7 * t * t) / (1.0 + 8.42420235e-4 * t + 7.08145163e-7 * t * t);
	float v = (0.317398726 + 4.22806245e-5 * t + 4.20481691e-8 * t * t) / (1.0 - 2.89741816e-5 * t + 1.61456053e-7 * t * t);
	return float2(u, v);
}

// Derivative of planckianLocusUV, Wolfram Alpha used
float2 planckianLocusUVDerivative(float temperature)
{
	float t = temperature;
	float u_denominator = 1.41213984e6 + 1189.61518 * t + t * t;
	float v_denominator = 6.19363586e6 - 179.45553 * t + t * t;
	float du = (-1.13758118e9 - 1.91615621e6 * t - 1.53177002 * t * t) / (u_denominator * u_denominator);
	float dv = (1.97471537e9 - 705674.106 * t - 308.606584 * t * t) / (v_denominator * v_denominator);
	return float2(du, dv);
}

// White of a black body using temperature and moved tint
float3 whiteXYZ(float temperature, float tint)
{
	float2 uv = planckianLocusUV(temperature);
	float2 tangent = normalize(planckianLocusUVDerivative(temperature));
	uv += float2(tangent.y, -tangent.x) * tint * 0.05f;

	return xyToXYZ(uvToXY(uv));
}

// The task is to convert current color temperature (given as argument) to be like D65 (WHITE) color
// This is done using 'Von Kries' method. It needs colors in LMS (long, medium, short) color space
// https://en.wikipedia.org/wiki/Color_balance#Von_Kries's_method
float3 whiteBalance(float3 color, float temperature, float tint)
{
	float3 source_white_lms = mul(XYZ_TO_BRADFORD_LMS, whiteXYZ(temperature, tint));
	float3 destination_white_lms = mul(XYZ_TO_BRADFORD_LMS, whiteXYZ(6500.0, 0.0));

	float3 color_lms = mul(XYZ_TO_BRADFORD_LMS, mul(REC709_TO_XYZ, color));

	color_lms *= destination_white_lms / source_white_lms;

	return mul(XYZ_TO_REC709, mul(BRADFORD_LMS_TO_XYZ, color_lms));
}

// Reference: aces-dev lib/ACESlib.Utilities_Color.ctl (calc_sat_adjust_matrix)
float3x3 ap1SaturationMatrix(float saturation)
{
	float3 luminance = AP1_TO_XYZ[1];
	float3x3 grey = float3x3(luminance, luminance, luminance);
	float3x3 identity = float3x3(1, 0, 0, 0, 1, 0, 0, 0, 1);
	return lerp(grey, identity, saturation);
}

// Khronos PBR Neutral Tone Mapper
// https://github.com/KhronosGroup/ToneMapping/tree/main/PBR_Neutral

// Input color is non-negative and resides in the Linear Rec. 709 color space.
// Output color is also Linear Rec. 709, but in the [0, 1] range.
float3 PBRNeutralToneMapping(float3 color) {
  const float startCompression = 0.8 - 0.04;
  const float desaturation = 0.15;

  float x = min(color.r, min(color.g, color.b));
  float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
  color -= offset;

  float peak = max(color.r, max(color.g, color.b));
  if (peak < startCompression) return color;

  const float d = 1. - startCompression;
  float newPeak = 1. - d * d / (peak + d - startCompression);
  color *= newPeak / peak;

  float g = 1. - 1. / (desaturation * (peak - newPeak) + 1.);
  return lerp(color, newPeak * float3(1, 1, 1), g);
}

// ACES Curve Fit Approximation (https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl)
static const float3x3 RRT_SAT_MAT = ap1SaturationMatrix(0.96);
static const float3x3 ODT_SAT_MAT = ap1SaturationMatrix(0.93);

float3 RRTAndODTFit(float3 v)
{
	float3 a = v * (v + 0.0245786f) - 0.000090537f;
	float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
	return a / b;
}

float3 ACESFitted(float3 color)
{
	color = mul(RRT_SAT_MAT, color);

	// Apply RRT and ODT
	color = RRTAndODTFit(color);

	color = mul(ODT_SAT_MAT, color);

	return color;
}

// Uchimura 2017, "HDR theory and practice" (aka GT 2017)
// Math: https://www.desmos.com/calculator/gslcdxvipg
// Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
float3 Uchimura(float3 x, float P, float a, float m, float l, float c, float b) {
  float l0 = ((P - m) * l) / a;
  float L0 = m - m / a;
  float L1 = m + (1.0 - m) / a;
  float S0 = m + l0;
  float S1 = m + a * l0;
  float C2 = (a * P) / (P - S1);
  float CP = -C2 / P;

  float3 w0 = float3(1.0 - smoothstep(0.0, m, x));
  float3 w2 = float3(step(m + l0, x));
  float3 w1 = float3(1.0 - w0 - w2);

  float3 T = m * pow(x / m, float3(c, c, c)) + b;
  float3 S = float3(P - (P - S1) * exp(CP * (x - S0)));
  float3 L = float3(m + a * (x - m));

  return T * w0 + L * w1 + S * w2;
}

// AgX (Blender AgX with Rec.2020 primaries)
// https://github.com/mrdoob/three.js/blob/dev/src/renderers/shaders/ShaderChunk/tonemapping_pars_fragment.glsl.js
static const float3x3 REC709_TO_REC2020 =
{
	{ 0.6274, 0.3293, 0.0433 },
	{ 0.0691, 0.9195, 0.0113 },
	{ 0.0164, 0.0880, 0.8956 }
};

static const float3x3 REC2020_TO_REC709 =
{
	{ 1.6605, -0.5876, -0.0728 },
	{ -0.1246, 1.1329, -0.0083 },
	{ -0.0182, -0.1006, 1.1187 }
};

static const float3x3 AGX_INSET =
{
	{ 0.856627153315983, 0.0951212405381588, 0.0482516061458583 },
	{ 0.137318972929847, 0.761241990602591, 0.101439036467562 },
	{ 0.11189821299995, 0.0767994186031903, 0.811302368396859 }
};

static const float3x3 AGX_OUTSET =
{
	{ 1.1271005818144368, -0.11060664309660323, -0.016493938717834573 },
	{ -0.1413297634984383, 1.157823702216272, -0.016493938717834257 },
	{ -0.14132976349843826, -0.11060664309660294, 1.2519364065950405 }
};

// https://iolite-engine.com/blog_posts/minimal_agx_implementation
// Mean error^2: 3.6705141e-06
float3 agxDefaultContrastApprox(float3 x)
{
	float3 x2 = x * x;
	float3 x4 = x2 * x2;

	return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

// Inputs and outputs are encoded as Linear-sRGB.
float3 AgX(float3 color)
{
	const float AgxMinEv = -12.47393; // log2(pow(2, LOG2_MIN) * MIDDLE_GRAY)
	const float AgxMaxEv = 4.026069; // log2(pow(2, LOG2_MAX) * MIDDLE_GRAY)

	color = mul(REC709_TO_REC2020, color);

	color = mul(AGX_INSET, color);

	// Log2 encoding
	color = max(color, 1e-10);
	color = log2(color);
	color = (color - AgxMinEv) / (AgxMaxEv - AgxMinEv);

	color = saturate(color);

	// Apply sigmoid
	color = agxDefaultContrastApprox(color);

	color = mul(AGX_OUTSET, color);

	// Linearize
	color = pow(max(0.0, color), 2.2);

	color = mul(REC2020_TO_REC709, color);

	return color;
}
