layout (set = 2, binding = 0) uniform UBO_default
{
	mat4 view;
	mat4 iview;
	mat4 projection;
	mat4 iprojection;
	vec4 camera_position;
	vec4 swapchain_size;
};

vec3 getVSPosition(vec2 uv, float hardware_depth)
{
    vec4 pos = inverse(projection) * vec4(uv * 2 - 1, hardware_depth, 1.0f);
    pos /= pos.w;
    return pos.xyz;
}