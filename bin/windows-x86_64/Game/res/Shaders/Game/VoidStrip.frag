#version 450
#include "Void.glsl"

layout(set = 0, binding = 1) uniform sampler2D SceneDepth;

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec4 inBody;
layout(location = 2) in vec3 inGlow;
layout(location = 3) flat in vec3 inLook;
layout(location = 4) in float inEnergy;

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
	vec3 radiance;
	float alpha;
	if (look == RibbonTendril)
	{
		float edge = 0.6 + 0.32 * VoidNoise(vec2(u * 2.6 - time * flow * 0.8, seed));
		float body = 1.0 - smoothstep(edge - 0.28, edge, av);
		float rim = exp(-pow((av - edge + 0.1) / 0.07, 2.0));
		float streaks = pow(VoidNoise(vec2(u * 4.5 - time * flow * 2.5, v * 2.2 + seed)), 3.0);
		float glints = smoothstep(0.78, 0.95, VoidNoise(vec2(u * 9.0 - time * flow * 4.0, v * 1.5 + seed * 1.7)));
		radiance = inGlow * (rim * 0.45 + (streaks * 0.5 + glints * 1.1) * body) + inBody.rgb * body;
		alpha = inBody.a * body;
	}
	else if (look == RibbonStream)
	{
		float core = exp(-av * av * 16.0);
		float halo = exp(-av * av * 3.0) * (1.0 - av * av);
		float dashes = 0.3 + 0.7 * smoothstep(0.3, 0.75, VoidNoise(vec2(u * 3.5 - time * flow * 5.0, seed)));
		radiance = inGlow * (core * 0.85 + halo * 0.35) * dashes + inBody.rgb * halo * 0.5;
		alpha = inBody.a * halo * 0.5;
	}
	else
	{
		radiance = inGlow * exp(-av * av * 9.0);
		alpha = 0.0;
	}
	vec2 screen = gl_FragCoord.xy / pc.Screen.xy;
	float sceneZ = VoidLinearDepth(textureLod(SceneDepth, screen, 0.0).r);
	float selfZ = VoidLinearDepth(gl_FragCoord.z);
	float fade = clamp((sceneZ - selfZ) / pc.Screen.w, 0.0, 1.0) * inEnergy;
	outScene = vec4(radiance, alpha) * fade;
	outEmission = vec4(radiance * pc.Camera.w, alpha) * fade;
}
