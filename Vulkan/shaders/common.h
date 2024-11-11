// Constants
const float PI = 3.14159265359;
const float TwoPI = 2 * PI;
const float Epsilon = 0.00001;

layout (set = 2, binding = 0) uniform UBO_default
{
	mat4 view;
	mat4 iview;
	mat4 projection;
	mat4 iprojection;
	vec4 camera_position;
	vec4 swapchain_size;
	float time;
};

vec3 getVSPosition(vec2 uv, float hardware_depth)
{
    vec4 pos = inverse(projection) * vec4(uv * 2 - 1, hardware_depth, 1.0f);
    pos /= pos.w;
    return pos.xyz;
}

void computeBasis(vec3 N, out vec3 T, out vec3 B)
{
    T = cross(N, vec3(0, 1, 0));
    T = mix(cross(N, vec3(1, 0, 0)), T, step(Epsilon, dot(T, T)));

    T = normalize(T);
    B = normalize(cross(N, T));
}

#ifdef COMPUTE_SHADER
// Converts cube face coordinates to a direction vector in world space
vec3 getCubemapNormal(vec2 resolution)
{
	vec2 st = gl_GlobalInvocationID.xy / vec2(resolution);
    vec2 uv = 2.0 * vec2(st.x, 1.0 - st.y) - vec2(1.0);

    vec3 normal = vec3(0.0);

    // Determine the normal based on the face
    switch (gl_GlobalInvocationID.z)
    {
        case 0:  // +X face
            normal = vec3(1.0, uv.y, -uv.x);
            break;
        case 1:  // -X face
            normal = vec3(-1.0, uv.y, uv.x);
            break;
        case 2:  // +Y face
            normal = vec3(uv.x, 1.0, -uv.y);
            break;
        case 3:  // -Y face
            normal = vec3(uv.x, -1.0, uv.y);
            break;
        case 4:  // +Z face
            normal = vec3(uv.x, uv.y, 1.0);
            break;
        case 5:  // -Z face
            normal = vec3(-uv.x, uv.y, -1.0);
            break;
    }

    // Ensure the vector is normalized (points on a unit sphere)
    return normalize(normal);
}
#endif

vec4 SRGBtoLinear(vec4 srgb)
{
    return vec4(pow(srgb.rgb, vec3(2.2)), srgb.a);
}

vec4 LinearToSRGB(vec4 linear)
{
    return vec4(pow(linear.rgb, vec3(1.0/2.2)), linear.a);
}