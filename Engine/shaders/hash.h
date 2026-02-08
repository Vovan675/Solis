#pragma once
// From https://www.shadertoy.com/view/4djSRW

float hash11(float p)
{
	p = frac(p * .1031);
	p *= p + 33.33;
	p *= p + p;
	return frac(p);
}

float hash12(float2 p)
{
	float3 p3  = frac(float3(p.xyx) * .1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return frac((p3.x + p3.y) * p3.z);
}

float hash13(float3 p3)
{
	p3  = frac(p3 * .1031);
	p3 += dot(p3, p3.zyx + 31.32);
	return frac((p3.x + p3.y) * p3.z);
}

float hash14(float4 p4)
{
	p4 = frac(p4  * float4(.1031, .1030, .0973, .1099));
	p4 += dot(p4, p4.wzxy+33.33);
	return frac((p4.x + p4.y) * (p4.z + p4.w));
}

float2 hash21(float p)
{
	float3 p3 = frac(float3(p, p, p) * float3(.1031, .1030, .0973));
	p3 += dot(p3, p3.yzx + 33.33);
	return frac((p3.xx+p3.yz)*p3.zy);
}

float2 hash22(float2 p)
{
	float3 p3 = frac(float3(p.xyx) * float3(.1031, .1030, .0973));
	p3 += dot(p3, p3.yzx+33.33);
	return frac((p3.xx+p3.yz)*p3.zy);

}

float2 hash23(float3 p3)
{
	p3 = frac(p3 * float3(.1031, .1030, .0973));
	p3 += dot(p3, p3.yzx+33.33);
	return frac((p3.xx+p3.yz)*p3.zy);
}

float3 hash31(float p)
{
	float3 p3 = frac(float3(p, p, p) * float3(.1031, .1030, .0973));
	p3 += dot(p3, p3.yzx+33.33);
	return frac((p3.xxy+p3.yzz)*p3.zyx); 
}

float3 hash32(float2 p)
{
	float3 p3 = frac(float3(p.xyx) * float3(.1031, .1030, .0973));
	p3 += dot(p3, p3.yxz+33.33);
	return frac((p3.xxy+p3.yzz)*p3.zyx);
}

float3 hash33(float3 p3)
{
	p3 = frac(p3 * float3(.1031, .1030, .0973));
	p3 += dot(p3, p3.yxz+33.33);
	return frac((p3.xxy + p3.yxx)*p3.zyx);

}

float4 hash41(float p)
{
	float4 p4 = frac(float4(p, p, p, p) * float4(.1031, .1030, .0973, .1099));
	p4 += dot(p4, p4.wzxy+33.33);
	return frac((p4.xxyz+p4.yzzw)*p4.zywx);
	
}

float4 hash42(float2 p)
{
	float4 p4 = frac(float4(p.xyxy) * float4(.1031, .1030, .0973, .1099));
	p4 += dot(p4, p4.wzxy+33.33);
	return frac((p4.xxyz+p4.yzzw)*p4.zywx);

}

float4 hash43(float3 p)
{
	float4 p4 = frac(float4(p.xyzx)  * float4(.1031, .1030, .0973, .1099));
	p4 += dot(p4, p4.wzxy+33.33);
	return frac((p4.xxyz+p4.yzzw)*p4.zywx);
}

float4 hash44(float4 p4)
{
	p4 = frac(p4  * float4(.1031, .1030, .0973, .1099));
	p4 += dot(p4, p4.wzxy+33.33);
	return frac((p4.xxyz+p4.yzzw)*p4.zywx);
}

uint murmurHash(uint x)
{
  uint m = 0x5bd1e995;
  uint r = 24;

  uint h = 64684;
  uint k = x;

  k *= m;
  k ^= (k >> r);
  k *= m;
  h *= m;
  h ^= k;

  return h;
}

uint wangHash(uint a)
{
	a = (a ^ 61) ^ (a >> 16);
	a = a + (a << 3);
	a = a ^ (a >> 4);
	a = a * 0x27d4eb2d;
	a = a ^ (a >> 15);
	return a;
}

float4 unpackUintToFloat4(uint x)
{
	return float4(
		(x & 0xff) / 255.0,
		((x >> 8) & 0xff) / 255.0,
		((x >> 16) & 0xff) / 255.0,
		((x >> 24) & 0xff) / 255.0
	);
}

float3 colorHash(uint id)
{
	return lerp(0, 1, unpackUintToFloat4(murmurHash(id)).rgb);
}