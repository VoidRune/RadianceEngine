#version 450
#include "Glass.glsl"

layout(std430, set = 0, binding = 0) readonly buffer FrameBuffer { GlassFrame Frame; };

layout(push_constant) uniform PushConstants
{
	uint Pass;
} pc;

layout(location = 0) in vec3 inWorld;
layout(location = 1) in vec3 inBary;
layout(location = 2) in vec2 inPlane;
layout(location = 3) flat in vec3 inNormal;
layout(location = 4) flat in uint inIndex;
layout(location = 5) flat in vec4 inGlint;
layout(location = 6) flat in vec3 inSpot;

layout(location = 0) out vec4 outBack;

void main()
{
	outBack = vec4(normalize(inNormal), dot(inWorld - Frame.Eye.xyz, Frame.Forward.xyz));
}
