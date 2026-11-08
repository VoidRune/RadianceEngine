layout(push_constant) uniform PushConstants
{
	vec4 Metrics;
} pc;

#define SMAA_RT_METRICS pc.Metrics
#define SMAA_GLSL_4
#define SMAA_PRESET_HIGH
#include "SMAA.hlsl"

layout(location = 0) in vec2 inUv;
