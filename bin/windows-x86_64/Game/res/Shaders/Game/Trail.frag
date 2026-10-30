#version 450
#include "Trail.glsl"

layout(std430, set = 0, binding = 1) readonly buffer StyleBuffer { TrailStyle Styles[]; };
layout(set = 0, binding = 2) uniform sampler2D SceneDepth;

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inAge;
layout(location = 3) flat in uint inTrail;

layout(location = 0) out vec4 outScene;
layout(location = 1) out vec4 outEmission;

float Hash(vec2 p)
{
	vec3 q = fract(vec3(p.xyx) * 0.1031);
	q += dot(q, q.yzx + 33.33);
	return fract((q.x + q.y) * q.z);
}

float Noise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = Hash(i);
	float b = Hash(i + vec2(1.0, 0.0));
	float c = Hash(i + vec2(0.0, 1.0));
	float d = Hash(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float Glints(float s, float v, float seed, float density, float time)
{
	float cell = floor(s * density);
	float f = fract(s * density);
	float h = Hash(vec2(cell, seed));
	float y = (Hash(vec2(cell, seed + 7.13)) - 0.5) * 1.2;
	vec2 d = vec2((f - 0.5) * 2.0, (v - y) * 2.4);
	float star = max(1.0 - length(d), 0.0);
	star = star * star * star * (1.0 + 2.0 * max(1.0 - abs(d.x) * 9.0, 0.0) + 2.0 * max(1.0 - abs(d.y) * 9.0, 0.0));
	return star * step(0.45, h) * (0.55 + 0.45 * sin(time * 17.0 + h * 40.0));
}

void main()
{
	TrailStyle style = Styles[inTrail];
	uint look = uint(style.Look.x + 0.5);
	float time = pc.Eye.w;
	float seed = style.Pattern.z * 0.0137;
	float v = inUv.y;
	float v2 = v * v;
	float age = clamp(inAge.x, 0.0, 1.0);
	float s = inUv.x * style.Pattern.x + time * style.Pattern.y;
	float window = 1.0 - v2;
	float body = 0.0;
	float core = 0.0;
	if (look == TrailFlame)
	{
		float n = 0.65 * Noise(vec2(s * 3.0, v * 1.7 + seed)) + 0.35 * Noise(vec2(s * 7.0, v * 3.5 - seed));
		float reach = max(1.0 - (0.35 + 0.6 * age) * n, 0.08);
		body = smoothstep(0.0, 0.55, 1.0 - abs(v) / reach) * (0.6 + 0.4 * n);
		core = exp(-v2 * 9.0) * (1.0 - age) * (0.55 + 0.45 * n);
	}
	else if (look == TrailFrost)
	{
		body = exp(-v2 * 8.0) * 0.85 + exp(-v2 * 1.8) * 0.3 * Noise(vec2(s * 2.5, v * 2.0 + seed));
		core = Glints(s, v, seed, 4.0, time) * (1.0 - 0.6 * age);
	}
	else if (look == TrailObsidian)
	{
		body = exp(-v2 * 2.5) * (0.65 + 0.35 * Noise(vec2(s * 2.0, v + seed)));
		float sheen = (abs(v) - 0.5) / 0.16;
		core = exp(-sheen * sheen) * (1.0 - age) * (0.6 + 0.4 * Noise(vec2(s * 4.0, seed)));
	}
	else if (look == TrailChain)
	{
		float link = abs(fract(s) - 0.5) * 2.0;
		body = exp(-v2 * 4.0) * 0.4;
		core = exp(-v2 * 12.0) * smoothstep(0.25, 0.65, link) * (1.0 - 0.5 * age);
	}
	else if (look == TrailSmoke)
	{
		float n = Noise(vec2(s * 1.6, v * 1.3 + seed)) * 0.7 + Noise(vec2(s * 4.1, v * 2.9 - seed)) * 0.3;
		body = smoothstep(0.05, 0.7, window * (0.35 + 0.9 * n)) * (0.45 + 0.55 * age);
		core = Glints(s, v, seed, 5.0, time * 0.6) * (1.0 - age) * (1.0 - age) + exp(-v2 * 10.0) * (1.0 - age) * (1.0 - age) * 0.6;
	}
	else if (look == TrailWisp)
	{
		float y1 = 0.45 * sin(s * 2.3 + seed * 3.0);
		float y2 = -0.45 * sin(s * 1.7 + seed * 5.0 + 1.3);
		float a = (v - y1) / 0.13;
		float b = (v - y2) / 0.13;
		core = (exp(-a * a) + exp(-b * b)) * (1.0 - age);
		body = exp(-v2 * 3.0) * (0.25 + 0.3 * Noise(vec2(s * 1.8, v * 2.0 + seed)));
	}
	else
	{
		body = exp(-v2 * 3.5) * (0.75 + 0.25 * Noise(vec2(s * 1.5, v * 4.0 + seed)));
		core = exp(-v2 * 16.0) * (1.0 - age) * (1.0 - age);
	}
	body *= window;
	core *= window;

	vec2 screen = gl_FragCoord.xy * pc.Screen.zw;
	float near = pc.Camera.x;
	float far = pc.Camera.y;
	float sceneZ = near * far / (far - textureLod(SceneDepth, screen, 0.0).r * (far - near));
	float trailZ = near * far / (far - gl_FragCoord.z * (far - near));
	float fade = clamp((sceneZ - trailZ) / style.Look.w, 0.0, 1.0);
	float alpha = inColor.a * fade;
	float additive = style.Look.z;
	vec3 radiance = (inColor.rgb * body + style.Core.rgb * (core * style.Core.a)) * alpha;
	outScene = vec4(radiance, clamp(body * alpha, 0.0, 1.0) * (1.0 - additive));
	outEmission = vec4(radiance * (style.Look.y * 2.0), 0.0);
}
