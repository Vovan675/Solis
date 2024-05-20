const vec3 QUAD[6] = {
    vec3(-1.0, -1.0,  0.0),
    vec3(-1.0,  1.0,  0.0),
    vec3( 1.0, -1.0,  0.0),
    vec3(-1.0,  1.0,  0.0),
    vec3( 1.0,  1.0,  0.0),
    vec3( 1.0, -1.0,  0.0)
};

layout(location = 0) out vec2 outUV;

void main() {
    // TODO: fullscreen triangle
    vec3 fragPos = QUAD[gl_VertexIndex];
    outUV = (fragPos*0.5 + 0.5).xy;
    gl_Position = vec4(fragPos, 1.0);
}