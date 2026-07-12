#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) out vec2 pos;

vec2 positions[4] = vec2[](
    vec2(-1,-1),
    vec2( 1,-1),
    vec2( 1, 1),
    vec2(-1, 1)
);

void main()
{
    pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);
}