#version 450
#include "GBuffer.glsl"

layout(set = 0, binding = 0) uniform sampler2D HeroMask;
layout(set = 0, binding = 1) uniform sampler2D HeroDepth;
layout(set = 0, binding = 2) uniform sampler2D SceneDepth;
layout(set = 0, binding = 3) uniform sampler2D SceneEmission;

layout(push_constant) uniform PushConstants
{
	vec4 Clip;
} pc;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outScene;

const float Coverage = 0.2;
const float Margin = 0.12;
const ivec2 Ring[8] = ivec2[](ivec2(2, 0), ivec2(-2, 0), ivec2(0, 2), ivec2(0, -2), ivec2(1, 1), ivec2(-1, 1), ivec2(1, -1), ivec2(-1, -1));

float LinearDepth(float depth)
{
	return pc.Clip.x * pc.Clip.y / (pc.Clip.y - depth * (pc.Clip.y - pc.Clip.x));
}

void main()
{
	ivec2 pixel = ivec2(gl_FragCoord.xy);
	vec4 mask = texelFetch(HeroMask, pixel, 0);
	if (mask.a <= 0.0 || IsHeroTag(texelFetch(SceneEmission, pixel, 0).a))
		discard;
	if (LinearDepth(texelFetch(SceneDepth, pixel, 0).r) > LinearDepth(texelFetch(HeroDepth, pixel, 0).r) - Margin)
		discard;
	ivec2 limit = ivec2(pc.Clip.zw) - 1;
	float edge = 0.0;
	for (int i = 0; i < 8; i++)
		if (texelFetch(HeroMask, clamp(pixel + Ring[i], ivec2(0), limit), 0).a <= 0.0)
			edge = 1.0;
	float rim = clamp((mask.a - Coverage) / (1.0 - Coverage), 0.0, 1.0);
	float alpha = max(0.14 + 0.45 * rim, 0.8 * edge);
	outScene = vec4(mask.rgb * ((0.35 + 1.4 * rim + 1.1 * edge) * alpha), alpha * 0.7);
}
