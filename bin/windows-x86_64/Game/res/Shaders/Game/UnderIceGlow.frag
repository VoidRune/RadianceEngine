#version 450
#include "Lake.glsl"

layout(std430, set = 0, binding = 0) readonly buffer FrameBuffer { LakeFrame Frame; };

layout(location = 0) in vec3 inWorld;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec4 inColor;
layout(location = 4) flat in uvec2 inMaterial;
layout(location = 5) flat in uint inFlags;
layout(location = 6) in vec4 inTint;
layout(location = 7) flat in vec4 inGlow;
layout(location = 8) in vec3 inLocal;

layout(location = 0) out vec4 outColor;

const vec3 SheetTone = vec3(0.66, 0.9, 1.0);

void main()
{
	float strength = inColor.a * inTint.a;
	float kind = inUv.y;
	vec3 tone = inColor.rgb * inTint.rgb;
	float light = 0.42 + 0.58 * smoothstep(0.0, 1.0, Frame.Aurora.x);
	float stir = Frame.Beast.w * (1.0 - smoothstep(0.0, Frame.Beast.z, length(inWorld.xz - Frame.Beast.xy)));
	light += stir * stir * 2.2;
	tone = mix(tone, vec3(0.3, 1.0, 1.0), stir);
	if (kind > 7.5 && inWorld.y > Frame.Plane.x + 0.01)
		discard;
	float depth = max(Frame.Plane.x - inWorld.y, 0.0);
	float murk = exp(-depth * 0.035);
	if (kind > 13.5)
	{
		float r = length(vec2(inUv.x, kind - 15.0));
		float halo = exp(-r * r * 4.0) * (1.0 - smoothstep(0.75, 1.0, r));
		float pulse = 0.7 + 0.3 * sin(Frame.Eye.w * 0.9 + inWorld.x * 1.3 + inWorld.z * 0.9);
		outColor = vec4(inColor.rgb * (inColor.a * halo * pulse * murk * (1.0 + stir * 1.5)), 1.0);
		return;
	}
	if (kind > 10.5)
	{
		vec3 toEye = normalize(Frame.Eye.xyz - inWorld);
		float facing = abs(dot(normalize(inNormal), toEye));
		float pulse = 0.72 + 0.28 * sin(Frame.Eye.w * 1.1 + inUv.x * 6.2832 + inWorld.x * 0.4 + inWorld.z * 0.3);
		outColor = vec4(inColor.rgb * (inColor.a * (0.45 + 0.85 * facing) * pulse * murk * 1.6 * (1.0 + stir * 1.5)), 1.0);
		return;
	}
	if (kind > 7.5)
	{
		float r = length(vec2(inUv.x, kind - 9.0));
		float dot = 1.0 - smoothstep(0.0, 1.0, r);
		float pulse = 0.65 + 0.35 * sin(Frame.Eye.w * 0.8 + inWorld.x * 0.7 + inWorld.z * 0.5);
		outColor = vec4(inColor.rgb * (dot * inColor.a * pulse * (2.4 + stir * 2.0)), 1.0);
		return;
	}
	if (kind > 4.5)
	{
		float r = length(vec2(inUv.x, kind - 6.0));
		float dot = 1.0 - smoothstep(0.0, 1.0, r);
		float fade = smoothstep(-22.5, -20.0, inWorld.y) * (1.0 - smoothstep(-6.8, -4.9, inWorld.y));
		outColor = vec4(tone * (dot * dot * strength * fade * (0.55 + 0.45 * light)), 1.0);
		return;
	}
	if (kind > 1.5)
	{
		vec2 d = vec2(inUv.x, kind - 3.0);
		float r = length(d);
		float rim = smoothstep(1.0, 0.8, r) * (0.62 + 0.38 * smoothstep(0.45, 0.95, r));
		outColor = vec4(tone * (rim * strength * light), 1.0);
		return;
	}
	float streak = 0.55 + 0.45 * sin(inLocal.x * 41.0 + inLocal.z * 37.0) * sin(inLocal.x * 13.0 - inLocal.z * 17.0);
	float fall = inUv.y * inUv.y * (1.0 - inUv.x * inUv.x);
	outColor = vec4(tone * SheetTone * (strength * fall * mix(streak, 1.0, fall) * light), 1.0);
}
