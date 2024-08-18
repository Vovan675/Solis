
layout(location = 0) in vec4 inPos;

layout (binding = 0) uniform UBO 
{
	mat4 light_space_matrix;
	mat4 model;
	vec3 light_pos;
};

void main()
{
    float lightDistance = length(inPos.xyz - light_pos.xyz);
	#if LIGHT_TYPE == 0
		gl_FragDepth = lightDistance / 40.0f;
	#endif
}