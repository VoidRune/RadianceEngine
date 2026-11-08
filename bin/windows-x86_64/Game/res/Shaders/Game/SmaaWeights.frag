#version 450
#include "Smaa.glsl"

layout(set = 0, binding = 0) uniform sampler2D Edges;
layout(set = 0, binding = 1) uniform sampler2D Area;
layout(set = 0, binding = 2) uniform sampler2D Search;

layout(location = 0) out vec4 outWeights;

void main()
{
	vec2 pixel;
	vec4 offset[3];
	SMAABlendingWeightCalculationVS(inUv, pixel, offset);
	outWeights = SMAABlendingWeightCalculationPS(inUv, pixel, offset, Edges, Area, Search, vec4(0.0));
}
