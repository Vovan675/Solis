layout (set = 2, binding = 0) uniform UBO_default
{
	mat4 view;
	mat4 iview;
	mat4 projection;
	mat4 iprojection;
	vec4 camera_position;
	vec4 swapchain_size;
};