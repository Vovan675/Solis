#include "common.h"

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

layout (set=1, binding=0) uniform sampler2D textures[];

layout(set=0, binding=0) uniform UBO
{
	uint composite_final_tex_id;
	uint normal_tex_id;
	uint depth_tex_id;
};


vec2 GetUV(vec3 view_pos)
{
    vec4 proj_position = projection * vec4(view_pos, 1.0);
    proj_position.xy /= proj_position.w;
    return proj_position.xy * 0.5f + 0.5f;
}

float GetDepth(vec2 depth_uv)
{
    //return -texture(textures[normal_tex_id], depth_uv).r;
    float n = 0.1;
    float f = 200.0;
    float depth = texture(textures[depth_tex_id], depth_uv).r;
    float eye_depth = 1.0 / ((1/f - 1/n) * depth + (1/n) );
    //return -eye_depth;
    //return nativeDepthToLinear(depth_uv, texture(textures[depth_tex_id], depth_uv).r);
    return getVSPosition(depth_uv, texture(textures[depth_tex_id], depth_uv).r).z;
    //return texture(textures[depth_tex_id], depth_uv).r;
}

float ray_step = 1.2f;
int binary_steps = 16;
int steps = 64;

vec3 ssr_binary_search(vec3 step, vec3 current_view_pos)
{
	for (int j = 0; j < binary_steps; j++)
	{
		current_view_pos += step;

		vec2 current_uv = GetUV(current_view_pos);
		float d = texture(textures[depth_tex_id], current_uv).r;
		vec3 vs_pos = getVSPosition(current_uv, d);
		float depth_delta = current_view_pos.z - vs_pos.z;

		step *= 0.5f;
		if (depth_delta > 0.0)
			current_view_pos += step;
		else
			current_view_pos -= step;
	}
	vec2 current_uv = GetUV(current_view_pos);
	return vec3(current_uv, 0);
}

vec3 ssr()
{
    // SSR
    float depth = texture(textures[depth_tex_id], uv).r;
	if (depth == 1.0)
		return vec3(0, 0, 0);

    vec3 normal = texture(textures[normal_tex_id], uv).rgb;
    vec3 view_normal = (view * vec4(normalize(normal), 0)).xyz;
    view_normal = normalize(view_normal);

    vec3 view_pos = getVSPosition(uv, depth);
	//vec4 world_pos = inverse(view) * vec4(view_pos, 1.0);

    // Find Reflected camera dir
    vec3 view_dir = normalize(reflect(normalize(view_pos), view_normal));

    vec2 current_uv = vec2(0, 0);
    bool under = false, hit = false;

    //float max_dist = abs(view_pos.z) * (1.0 / sqrt(0.01 + clamp(dot(view_dir.xy, view_dir.xy), 0.0, 1.0)));
    float max_dist = abs(view_pos.z);
    //max_dist = 100;
    float incr = max_dist / float(steps);
    float incr_refine = incr / float(binary_steps);

    float edge = 0.0;
    for (float t = incr; t < max_dist && !hit && edge < 1.0; t+=incr)
    {
        vec3 p = view_pos + t * view_dir;
        current_uv = GetUV(p);
        //if (current_uv.x < 0.0 || current_uv.x > 1.0 || current_uv.y < 0.0 || current_uv.y > 1.0)
        //    break; // Out of bounds, stop marching
        float new_depth = GetDepth(current_uv);
        //vec3 new_view_pos = getVSPosition(current_uv, new_depth);
        //return vec3(new_view_pos.z, 0, 0);

        //float depth_delta = view_pos.z - new_view_pos.z;
        //return vec3(-view_pos.z, 0, 0);
        //return vec3(depth_delta > 0, 0, 0);
	    //vec4 new_world_pos = inverse(view) * vec4(new_view_pos, 1.0);
        //current_step = length(world_pos - new_world_pos);
        bool was_under = under;
        under = p.z < new_depth;
        if (!was_under && under)
        {
            float best = new_depth - p.z;
			float t2 = t - incr;
			for( int i=0; i<binary_steps; ++i )
			{
				t2 += incr_refine;
				vec3 p2 = view_pos + t2*view_dir;
				vec2 tc2 = GetUV(p2);

				float d2 = GetDepth(tc2);
				//d2 = getVSPosition(tc2, d2).z;///
                //if (GetDepth(tc2) == 1.0)
                //    d2 = 10000;
				float diff = d2 - p2.z;
				if( diff >= 0 && diff < best )
				{
					best = diff;
					current_uv = tc2;
				}
			}
			hit = best < (0.25*incr);
        }

		edge = max(abs(2.0*current_uv.x-1.0), abs(2.0*current_uv.y-1.0));
    }
	//current_uv = (floor(current_uv * swapchain_size.xy) + vec2(0.5,0.5)) * swapchain_size.zw;
    //return hit ? vec3(1, 0, 0) : vec3(0, 0, 0);
    //return vec3(-GetDepth(GetUV(view_pos)) / 10.0f, 0, 0);
    //return hit ? vec3(current_uv, 0) : vec3(0, 0, 0);

    //depth = texture(textures[depth_tex_id], current_uv).r;
	//if (depth > 0.9999)
	//	return vec3(1, 0, 0);
    return hit ? texture(textures[composite_final_tex_id], current_uv).rgb : vec3(0, 0, 0);
    //return texture(textures[composite_final_tex_id], current_uv).rgb;
}

void main()
{
    vec3 result = ssr();
	vec3 default_color = texture(textures[composite_final_tex_id], uv).rgb;
    outColor = vec4(result.xyz, 1);
    //outColor = vec4(result.xyz, 1);
}