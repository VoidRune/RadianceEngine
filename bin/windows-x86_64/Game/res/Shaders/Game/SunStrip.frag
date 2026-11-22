#version 450
#include "Sun.glsl"

layout(set = 0, binding = 1) uniform sampler2D SceneDepth;

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec4 inBody;
layout(location = 2) flat in vec3 inLook;
layout(location = 3) in float inEnergy;

layout(location = 0) out vec4 outScene;
layout(location = 1) out vec4 outEmission;

void main()
{
	float time = pc.Eye.w;
	uint look = uint(inLook.x + 0.5);
	float seed = inLook.y;
	float flow = inLook.z;
	float u = inUv.x;
	float v = inUv.y;
	float av = abs(v);
	float heat = inBody.x;
	float intensity = inBody.y;
	vec3 radiance;
	float alpha;
	if (look == SunPlasma)
	{
		float fibers = SunNoise(vec3(u * 1.1 - time * flow * 0.5, v * 6.0, seed));
		float threads = SunNoise(vec3(u * 0.6 - time * flow * 0.3, v * 14.0, seed + 7.0));
		float clumps = SunNoise(vec3(u * 2.4 - time * flow * 1.2, v * 1.5, seed * 1.7));
		float edge = 0.5 + 0.4 * SunNoise(vec3(u * 3.5 - time * flow * 0.9, seed, 3.1));
		float body = 1.0 - smoothstep(edge - 0.4, edge, av);
		float core = exp(-av * av * 5.0);
		float temp = 0.22 + 0.55 * heat * core + 0.2 * clumps * core + 0.1 * threads;
		float density = body * (0.35 + 0.45 * fibers + 0.35 * threads) * (0.55 + 0.6 * clumps);
		float halo = exp(-av * av * 1.6) * (1.0 - av * av);
		radiance = SunRamp(temp) * intensity * (density * (0.6 + 0.8 * heat) + halo * (0.22 + 0.2 * clumps));
		alpha = inBody.z * body * (0.5 + 0.5 * clumps);
	}
	else
	{
		float core = exp(-av * av * 7.0);
		radiance = SunRamp(0.45 + 0.6 * heat) * intensity * core * (0.8 + heat);
		alpha = 0.0;
	}
	vec2 screen = gl_FragCoord.xy / pc.Screen.xy;
	float sceneZ = SunLinearDepth(textureLod(SceneDepth, screen, 0.0).r);
	float selfZ = SunLinearDepth(gl_FragCoord.z);
	float fade = clamp((sceneZ - selfZ) / pc.Screen.w, 0.0, 1.0) * inEnergy;
	outScene = vec4(radiance, alpha) * fade;
	outEmission = vec4(radiance * pc.Camera.w, alpha) * fade;
}
