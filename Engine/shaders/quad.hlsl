static const float3 QUAD[6] =
{
	float3(-1.0, -1.0, 0.0),
    float3(-1.0, 1.0, 0.0),
    float3(1.0, -1.0, 0.0),
    float3(-1.0, 1.0, 0.0),
    float3(1.0, 1.0, 0.0),
    float3(1.0, -1.0, 0.0)
};

struct VSOutput
{
	float2 uv : TEXCOORD0;
	float4 position : SV_POSITION;
};

VSOutput VSMain(uint vertexID : SV_VertexID)
{
	VSOutput output;

    // Get the vertex position from the QUAD array
	float3 fragPos = QUAD[vertexID];

    // Calculate the UV coordinates
	output.uv = (fragPos * 0.5 + 0.5).xy;
	output.uv.y = 1.0f - output.uv.y;

    // Set the vertex position
	output.position = float4(fragPos, 1.0);
	
    return output;
}