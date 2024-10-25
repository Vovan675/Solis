
layout(location = 0) in vec4 inPos;

layout (binding = 0) uniform UBO 
{
	mat4 light_space_matrix;
	vec4 light_pos;
	float z_far;
	float padding_1;
	float padding_2;
	float padding_3;
};

void main()
{
	#if LIGHT_TYPE == 0
    	float lightDistance = length(inPos.xyz - light_pos.xyz);
		gl_FragDepth = lightDistance / z_far;
	#endif
}