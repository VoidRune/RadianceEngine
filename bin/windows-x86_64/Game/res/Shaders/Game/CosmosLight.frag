#version 450
#include "CosmosCommon.glsl"

layout(set = 0, binding = 1) uniform sampler2D Scene;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main()
{
	vec2 texel = vec2(textureSize(Scene, 0));
	vec2 reach = 22.0 / texel;
	vec3 sum = vec3(0.0);
	float total = 0.0;
	for (int i = 0; i < 16; i++)
	{
		float angle = float(i) * 2.3999632;
		float radius = sqrt((float(i) + 0.5) / 16.0);
		vec2 offset = vec2(cos(angle), sin(angle)) * radius * reach;
		float weight = 1.0 - 0.5 * radius;
		sum += textureLod(Scene, inUv + offset, 0.0).rgb * weight;
		total += weight;
	}
	vec3 color = Tonemap(sum / total * frame.Lens.z * 1.6) * frame.Lens.w;
	outColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
