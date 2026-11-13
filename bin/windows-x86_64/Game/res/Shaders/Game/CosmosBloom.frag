#version 450
#include "CosmosCommon.glsl"

layout(set = 0, binding = 1) uniform sampler2D Scene;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main()
{
	vec2 size = vec2(textureSize(Scene, 0));
	vec2 texel = 1.0 / size;
	vec3 sum = vec3(0.0);
	if (abs(size.x - frame.Screen.x) < 0.5)
	{
		for (int y = 0; y < 2; y++)
			for (int x = 0; x < 2; x++)
				sum += textureLod(Scene, inUv + (vec2(x, y) * 2.0 - 1.0) * texel, 0.0).rgb;
		sum *= 0.25;
		float luminance = dot(sum, vec3(0.3, 0.55, 0.15));
		outColor = vec4(sum * (max(luminance - 0.3, 0.0) / max(luminance, 1e-4)), 1.0);
		return;
	}
	float total = 0.0;
	for (int i = 0; i < 24; i++)
	{
		float angle = float(i) * 2.3999632;
		float radius = sqrt((float(i) + 0.5) / 24.0);
		float weight = exp(-radius * radius * 2.5);
		sum += textureLod(Scene, inUv + vec2(cos(angle), sin(angle)) * radius * 7.0 * texel, 0.0).rgb * weight;
		total += weight;
	}
	outColor = vec4(sum / total, 1.0);
}
