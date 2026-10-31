#version 450
#include "Glass.glsl"
#include "Dissolve.glsl"

layout(std430, set = 0, binding = 0) readonly buffer FrameBuffer { GlassFrame Frame; };

layout(location = 0) in vec3 inWorld;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec4 inTangent;
layout(location = 4) flat in uint inMaterial;
layout(location = 5) in vec3 inLocal;
layout(location = 6) flat in vec4 inDissolve;
layout(location = 7) flat in vec4 inBurn;

layout(location = 0) out vec4 outBack;

void main()
{
	if (inDissolve.x > 0.0 && DissolveField(inLocal, inDissolve.z, inDissolve.w, inBurn.w) <= DissolveThreshold(inDissolve.x, inDissolve.y))
		discard;
	outBack = vec4(normalize(inNormal), dot(inWorld - Frame.Eye.xyz, Frame.Forward.xyz));
}
