#version 450
#include "Blast.glsl"

layout(set = 0, binding = 1) uniform sampler2D SceneDepth;

layout(location = 0) in vec3 inWorld;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inCoords;
layout(location = 3) flat in vec4 inInfo;
layout(location = 4) flat in vec4 inTimes;
layout(location = 5) flat in vec4 inCenter;

layout(location = 0) out vec4 outScene;
layout(location = 1) out vec4 outEmission;

vec3 FireRamp(float heat, float violet)
{
	vec3 soot = mix(vec3(0.035, 0.025, 0.022), vec3(0.03, 0.015, 0.045), clamp(violet * 1.8, 0.0, 1.0));
	vec3 ember = mix(vec3(0.3, 0.04, 0.015), vec3(0.11, 0.035, 0.14), clamp(violet * 1.8, 0.0, 1.0));
	vec3 red = mix(vec3(0.85, 0.15, 0.03), vec3(0.32, 0.08, 0.62), clamp(violet * 1.5, 0.0, 1.0));
	vec3 orange = mix(vec3(1.0, 0.44, 0.08), vec3(0.56, 0.27, 0.92), clamp(violet * 1.2, 0.0, 1.0));
	vec3 yellow = mix(vec3(1.0, 0.76, 0.32), vec3(0.86, 0.62, 1.0), violet * violet);
	vec3 white = mix(vec3(1.0, 0.95, 0.8), vec3(1.0, 0.9, 1.0), violet * violet * violet);
	float aa = max(fwidth(heat), 0.01);
	vec3 color = soot;
	color = mix(color, ember, smoothstep(0.04, 0.16, heat));
	color = mix(color, red, smoothstep(0.24 - aa, 0.27 + aa, heat));
	color = mix(color, orange, smoothstep(0.42 - aa, 0.45 + aa, heat));
	color = mix(color, yellow, smoothstep(0.6 - aa, 0.63 + aa, heat));
	color = mix(color, white, smoothstep(0.8 - aa, 0.83 + aa, heat));
	return color * (1.0 + 0.7 * smoothstep(0.82, 1.05, heat));
}

float Swirl(float a, float s, float t, float seed)
{
	float shear = inTimes.w;
	float coarse = BlastLoopNoise(vec2(a * 7.0 + s * 4.5 * shear, s * 15.0 - t * 2.5 + seed), 7.0);
	float fine = BlastLoopNoise(vec2(a * 16.0 + s * 9.0 * shear, s * 30.0 - t * 4.0 + seed * 1.3), 16.0);
	return 0.65 * coarse + 0.35 * fine;
}

float WorldSwirl(float t, float seed)
{
	vec2 d = inWorld.xz - inCenter.xz;
	float r = length(d) / inCenter.w;
	float a = atan(d.y, d.x) / BlastTau + 0.5 - inTimes.w * (0.04 + 0.22 * BlastEaseOut(t / 0.5)) / BlastTau;
	return Swirl(a, 0.9 * r, t, seed);
}

void main()
{
	float piece = inInfo.x;
	float violet = inInfo.z;
	float seed = inInfo.w;
	vec3 n = normalize(inNormal);
	vec3 view = normalize(pc.Eye.xyz - inWorld);
	vec3 light = normalize(vec3(0.0, 1.0, 0.0) + 0.35 * normalize(vec3(view.x, 0.0, view.z) + vec3(1e-4)) + vec3(-0.3, 0.0, 0.15));
	float lit = dot(n, light);
	vec3 radiance;
	vec3 glow;
	float alpha;
	if (piece > 2.5)
	{
		float fire = inTimes.x;
		float b = inCoords.y;
		float life = inCoords.z;
		float density = inCoords.w;
		float thin = density + 0.1 - 1.1 * smoothstep(0.35, 1.0, life);
		float edge = smoothstep(0.1, 0.85, abs(dot(n, view)));
		alpha = 0.6 * inTimes.z * smoothstep(0.0, 0.3, thin) * edge * smoothstep(0.0, 0.15, life);
		float alive = fire < 1.5 ? 1.0 - smoothstep(0.2, 1.0, fire) : 0.0;
		vec3 body = mix(vec3(0.07, 0.06, 0.06), vec3(0.065, 0.05, 0.08), violet) * (0.6 + 0.5 * clamp(lit, 0.0, 1.0));
		glow = FireRamp(0.45, violet) * (0.6 * alive * b);
		radiance = body + glow;
	}
	else
	{
		float t = inInfo.y;
		float flash = exp(-t * 18.0);
		float cool = 0.1 + 0.8 * smoothstep(0.04, 1.0, t);
		float heat;
		float mask;
		if (piece < 0.5)
		{
			float s = inCoords.y;
			float strand = Swirl(inCoords.x, s, t, seed);
			float licks = smoothstep(0.36, 0.64, strand);
			float gloss = pow(max(dot(n, normalize(light + view)), 0.0), 12.0);
			heat = inCoords.z + 0.42 * smoothstep(0.05, 0.35, s) * (licks - 0.5) + 0.15 * (lit - 0.55) + 0.28 * gloss;
			mask = 0.7 * strand + 0.3 * inCoords.w + 0.2 - 1.1 * smoothstep(0.42, 1.0, t) - 0.6 * (1.0 - s) * smoothstep(0.25, 0.75, t);
		}
		else if (piece < 1.5)
		{
			float lump = inCoords.w;
			float streak = WorldSwirl(t, seed);
			float licks = smoothstep(0.36, 0.64, streak);
			float facing = clamp(dot(n, view), 0.0, 1.0);
			heat = inCoords.z + 0.4 * (licks - 0.5) + 0.15 * (lump - 0.5) + 0.25 * (facing - 0.55) + 0.1 * (lit - 0.5);
			mask = 0.6 * streak + 0.4 * lump + 0.24 - 1.2 * smoothstep(0.38, 1.0, t) - 0.45 * (1.0 - facing) * smoothstep(0.1, 0.55, t);
		}
		else
		{
			float u = inCoords.x;
			float v = inCoords.y;
			float id = inCoords.w;
			float body = 1.0 - abs(2.0 * v - 1.0);
			float strand = BlastNoise(vec2(u * 3.5 + id * 5.0, v * 4.5 + id));
			float rag = 0.06 + 0.28 * BlastNoise(vec2(u * 9.0 + id * 7.0 - t * 6.0, v * 1.5 + id * 3.0));
			heat = inCoords.z + 0.45 * (strand - 0.5) - 0.25 * smoothstep(0.1, 0.4, t);
			mask = body - rag - 0.25 * smoothstep(0.62, 0.85, strand) * smoothstep(0.15, 0.5, u) - 1.1 * smoothstep(0.45, 1.0, t);
		}
		heat += 0.75 * flash - cool;
		heat -= 0.3 * (1.0 - smoothstep(0.0, 0.18, mask));
		alpha = smoothstep(0.0, max(fwidth(mask) * 1.5, 0.015), mask);
		radiance = FireRamp(heat, violet);
		glow = radiance;
	}
	vec2 screen = gl_FragCoord.xy / pc.Screen.xy;
	float sceneZ = BlastLinearDepth(textureLod(SceneDepth, screen, 0.0).r);
	float selfZ = BlastLinearDepth(gl_FragCoord.z);
	alpha *= clamp((sceneZ - selfZ) / pc.Camera.z, 0.0, 1.0);
	if (alpha < 0.01)
		discard;
	outScene = vec4(radiance * alpha, alpha);
	outEmission = vec4(glow * (pc.Camera.w * alpha), alpha);
}
