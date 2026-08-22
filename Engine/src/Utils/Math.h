#pragma once

namespace Math
{
	inline uint16_t pack32to16(float f)
	{
		uint32_t x;
		memcpy(&x, &f, 4);

		uint16_t h = uint16_t((x >> 16) & 0x8000u);
		uint32_t e = (x >> 23) & 0xffu;
		uint32_t m = x & 0x7fffffu;
		if (e == 0)
			return h;
		if (e == 255)
			return uint16_t(h | (m ? 0x7e00u : 0x7c00u));
		int ne = int(e) - 127 + 15;
		if (ne >= 31)
			return uint16_t(h | 0x7c00u);
		if (ne <= 0)
		{
			if (ne < -10)
				return h;
			m = (m | 0x800000u) >> (1 - ne);
			return uint16_t(h | (m >> 13));
		}
		return uint16_t(h | uint16_t(ne << 10) | uint16_t(m >> 13));
	}

	inline float unpack16to32(uint16_t h)
	{
		uint32_t s = uint32_t(h & 0x8000u) << 16;
		uint32_t e = (h >> 10) & 0x1fu;
		uint32_t m = uint32_t(h & 0x3ffu);
		uint32_t r;
		if (e == 0)
			r = s | (m << 13);
		else if (e == 31)
			r = s | 0x7f800000u | (m << 13);
		else
			r = s | ((e + 112u) << 23) | (m << 13);

		float f;
		memcpy(&f, &r, 4);
		return f;
	}

	glm::mat4 getCubeFaceTransform(int face_index);

	template<typename T>
	T alignedSize(T value, T alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}

	uint32_t divideRoundUp(uint32_t nominator, uint32_t denominator);
	uint64_t roundUp(uint64_t size, uint64_t granularity);

	bool isPowerOfTwo(uint32_t x);

	inline float halton(uint32_t index, uint32_t base)
	{
		float result = 0.0f;
		float fraction = 1.0f;
		while (index > 0)
		{
			fraction /= base;
			result += fraction * (index % base);
			index /= base;
		}
		return result;
	}

	inline glm::vec2 halton2D(uint32_t index)
	{
		return {halton(index, 2), halton(index, 3)};
	}

	// Get how much nits would be WHITE on current "camera" when EV100 = 0
	inline float getLuminanceMax()
	{
		// https://en.wikipedia.org/wiki/Film_speed#Measurements_and_calculations
		//float lens_attenuation = 0.78f; // ideal lens (pi/4)
		float lens_attenuation = 0.65f; // more typical lens (pi/4 * vignette * cosine angle etc)

		float iso_constant = 0.78f;
		float luminance_max = iso_constant / lens_attenuation;
		return luminance_max;
	}

	// luminance_max - maximum nits needed to fully saturate sensor
	inline float ev100ToLuminance(float luminance_max, float ev100)
	{
		return luminance_max * pow(2.0f, ev100);
	}

	inline float luminanceToEV100(float luminance_max, float luminance)
	{
		return log2(luminance / luminance_max);
	}

	inline float cameraToEV100(float aperture, float shutter_speed, float iso)
	{
		// https://en.wikipedia.org/wiki/Exposure_value
		return log2(aperture * aperture / shutter_speed) - log2(iso / 100.0f);
	}

}