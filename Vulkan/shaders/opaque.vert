#version 450

layout(set=0, binding=0) uniform UBO
{
	mat4 model;
	mat4 view;
	mat4 proj;
	vec3 cameraPosition;
} ubo;

layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUV;
layout(location=3) in vec3 inColor;

layout(location=0) out vec3 outPos;
layout(location=1) out vec3 outNormal;
layout(location=2) out vec2 outUV;
layout(location=3) out vec3 outColor;

void main()
{
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    
	outPos = vec3(ubo.model * vec4(inPosition, 1.0));
	outNormal = normalize(mat3(ubo.model) * inNormal); // Apply rotation to normal
	outUV = inUV;
	outColor = inColor;

    // Normal in world space???
	// mat3 mNormal = transpose(inverse(mat3(ubo.model)));
	// outNormal = mNormal * normalize(inNormal);	
	// outTangent = mNormal * normalize(inTangent);
}