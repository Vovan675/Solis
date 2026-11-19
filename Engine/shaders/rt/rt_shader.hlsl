#include "../ray_tracing.h"
#include "../lighting/lighting.h"

cbuffer Constants : register(b1)
{
	uint light_index;
	uint depth_texture_id;
	uint output_texture_id;
};

[numthreads(8, 8, 1)]
void CSMain(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(render_resolution.xy)))
		return;

	RWTexture2D<float4> output = ResourceDescriptorHeap[output_texture_id];

	Texture2D<float> depth_texture = ResourceDescriptorHeap[depth_texture_id];
	float depth = depth_texture.Load(int3(pixel, 0)).r;
	float3 world_pos = GetWSPosition((pixel + 0.5f) * render_resolution.zw, depth);

	Light light = getLight(light_index);

	RayDesc ray;
	ray.Origin = world_pos;
	ray.Direction = normalize(light.direction.xyz);
	ray.TMin = 0.005;
	ray.TMax = 10000.0;

	output[pixel] = float4(traceVisibility(ray) ? 1 : 0, 0, 0, 0);
}
