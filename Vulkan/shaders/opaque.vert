#include "common.h"

layout(push_constant) uniform constants
{
	mat4 model;
	vec4 albedo;
	int albedo_tex_id;
	int metalness_tex_id;
	int roughness_tex_id;
	int specular_tex_id;
	vec4 shading;
	int normal_tex_id;
	int padding[3];
};

layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec3 inTangent;
layout(location=3) in vec2 inUV;
layout(location=4) in vec3 inColor;

layout(location=0) out vec3 outPos;
layout(location=1) out vec3 outNormal;
layout(location=2) out vec2 outUV;
layout(location=3) out vec3 outColor;
layout(location=4) out mat3 outTBN;

void main()
{
	gl_Position = projection * view * model * vec4(inPosition, 1.0);
    
	outPos = (model * vec4(inPosition, 1.0)).xyz;
	outNormal = normalize(transpose(inverse(mat3(model))) * inNormal); // Apply rotation to normal
	outUV = inUV;
	outColor = inColor;

	vec3 tangent = normalize(inTangent - dot(inTangent, inNormal) * inNormal);
	vec3 bitangent = cross(inNormal, tangent);
	outTBN = transpose(inverse(mat3(model))) * mat3(tangent, bitangent, inNormal);

    // Normal in world space???
	// mat3 mNormal = transpose(inverse(mat3(ubo.model)));
	// outNormal = mNormal * normalize(inNormal);	
	// outTangent = mNormal * normalize(inTangent);
}