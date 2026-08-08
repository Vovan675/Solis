#pragma once
#include "common.h"
#include "random.h"

// ============================================================================
// Russian Roulette (NVIDIA-style)
// ============================================================================

bool HandleRussianRoulette(float3 throughput, int bounce, int max_bounces, inout float rr_correction, inout RandomState rng)
{
	float rrVal = sqrt(Luminance(throughput)); // closer to perceptual
	
	float prob = saturate(0.85 - rrVal);
	prob = prob * prob;
	
    // Start stochastically terminating paths from 0.4 bounce limit, with increasing probability up to 0.6 (1-0.4)
	float bounce_factor = max(0.0, (float(bounce) / float(max_bounces)) - 0.4);
	prob = saturate(prob + bounce_factor);
	
	if (Random(rng) < prob)
		return true;
	
	rr_correction = 1.0 / (1.0 - prob);
	return false;
}

// ============================================================================
// Firefly Reduction (NVIDIA-style)
// ============================================================================

// Experimental ray cone spread heuristic: assume pdf comes from an uniform sphere cap lobe. Then we can compute cone spread
// angle alpha (a plane angle) from the uniform sphere cap solid angle (omega), which can be derived from pdf 
// (omega = 1 / uniform_sphere_cap_pdf). 
// The formula is alpha = 2 * acos( 1 - omega / 2*PI ) - see https://rechneronline.de/winkel/solid-angle.php
// (This heuristic starts to break down for BSDFs with overlapping lobes but seems good enough in most cases - perhaps BSDF should be responsible providing the scatter angle).
//
// growthFactor 0.3 is very conservative underestimation, see https://www.jcgt.org/published/0010/01/01/paper.pdf, "Improved Shader and Texture Level of Detail Using Ray Cones", 
// Chapter 3. Curvature Approximations            "...On the other hand, when ray cones are used inside a Monte Carlo path tracer, one would prefer slightly underestimating the 
// spread angle, since antialiasing will be handled by stochastic supersampling anyway, and the main objective would be to avoid introducing overblur in the results."
float ComputeRayConeSpreadAngleExpansionByScatterPDF(float pdf, float unused)
{
	const float minPDF = 0.0001;
	pdf = max(pdf, minPDF);
	return sqrt(1.0 / pdf);
}

// Ad-hoc heuristic: reduce firefly threshold the more we bounce, but make it dependent on pdf and lobe probability
float ComputeNewScatterFireflyFilterK(float currentK, float bouncePDF, float lobeP)
{
	const float minK = 0.00001;
	float angle = (bouncePDF == 0.0) ? 0.0 : ComputeRayConeSpreadAngleExpansionByScatterPDF(bouncePDF, 1.0);
	
	const float k = 32.0;
	float p = k / (k + angle * angle);
	p *= sqrt(max(lobeP, 0.0001));
	
	return max(minK, currentK * p);
}

// Experimental: Biased cap to maximum radiance based on current vs starting ray cone spread angle, used as a rough estimate of probability of the path.
float3 FireflyFilter(float3 signal, float threshold, float fireflyFilterK)
{
	float dynamicThreshold = threshold * fireflyFilterK;
	float luminance = max(1e-7, Luminance(signal));

	if (luminance > dynamicThreshold)
		signal = signal / luminance * dynamicThreshold;

	return signal;
}

float FireflyFilterShort(float signalAverage, float threshold, float fireflyFilterK)
{
	float dynamicThreshold = threshold * fireflyFilterK;
	return (signalAverage > dynamicThreshold) ? (dynamicThreshold / signalAverage) : 1.0;
}

