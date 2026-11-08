#version 450
#include "Smaa.glsl"

layout(set = 0, binding = 0) uniform sampler2D Color;
layout(set = 0, binding = 1) uniform sampler2D Weights;

layout(location = 0) out vec4 outColor;

void main()
{
	vec4 offset;
	SMAANeighborhoodBlendingVS(inUv, offset);
	outColor = vec4(SMAANeighborhoodBlendingPS(inUv, offset, Color, Weights).rgb, 1.0);
}
