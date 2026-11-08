#version 450
#include "Smaa.glsl"

layout(set = 0, binding = 0) uniform sampler2D Color;

layout(location = 0) out vec2 outEdges;

void main()
{
	vec4 offset[3];
	SMAAEdgeDetectionVS(inUv, offset);
	outEdges = SMAALumaEdgeDetectionPS(inUv, offset, Color);
}
