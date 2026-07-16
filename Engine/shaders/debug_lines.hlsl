#include "common.h"

struct VSInput {
    float4 position : POSITION;
    float3 color : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    bool is_screenspace = input.position.a > 0;

    if (is_screenspace)
    {
        output.position = float4(input.position.xyz, 1.0f);
    } else
    {
        output.position = mul(view_projection_unjittered, float4(input.position.xyz, 1.0f));
    }
    output.color = input.color;
    return output;
}

struct PSOutput {
    float4 color : SV_Target;
};

PSOutput PSMain(VSOutput input)
{
    PSOutput output;
    output.color = float4(input.color, 1);
    return output;
}

cbuffer Uniforms : register(b0)
{
	uint lines_draw_args_buffer_id;
};

[numthreads(1, 1, 1)]
void CSGenerateDrawCalls(uint3 dispatchID : SV_DispatchThreadID)
{
    RWByteAddressBuffer gpu_lines = ResourceDescriptorHeap[lines_gpu_buffer_id];
    uint lines_count = gpu_lines.Load(0);
    gpu_lines.Store(0, 0);

    RWStructuredBuffer<DrawIndirect> draw_args = ResourceDescriptorHeap[lines_draw_args_buffer_id];
    draw_args[0] = (DrawIndirect)0;
    draw_args[0].vertex_count_per_instance = 2 * lines_count;
    draw_args[0].instance_count = 1;
}

VSOutput VSGpuLines(uint vertex_id : SV_VertexID)
{
    VSOutput output;

    ByteAddressBuffer gpu_lines = ResourceDescriptorHeap[lines_gpu_buffer_id];
    GpuLine input = gpu_lines.Load<GpuLine>(4 + vertex_id * sizeof(GpuLine));

    bool is_screenspace = input.position.a > 0;

    if (is_screenspace)
    {
        output.position = float4(input.position.xyz, 1.0f);
    } else
    {
        output.position = mul(view_projection_unjittered, float4(input.position.xyz, 1.0f));
    }
    output.color = input.color;
    return output;
}