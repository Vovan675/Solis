#include "common.h"
#include "fxaa.h"

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=0) uniform UBO
{
	uint composite_final_tex_id;
} ubo;

void main() {
    vec2 uv = inUV;
    
    vec4 value = vec4(1, 1, 1, 1);
    vec4 composite_final = texture(textures[ubo.composite_final_tex_id], uv);

	vec4 zero = vec4(0, 0, 0, 0);
	
	#define TEX textures[ubo.composite_final_tex_id]

    value = FxaaPixelShader(uv, zero, TEX, TEX, TEX,
							swapchain_size.zw, zero, zero, zero, 1.0f, 0.166f, 0.0833f, 0, 0, 0, zero);
    outColor = vec4(value.rgb, 1.0);
}