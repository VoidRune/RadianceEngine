#version 450
#include "Lightning.glsl"

layout(set = 0, binding = 1) uniform sampler2D SceneDepth;

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec3 inGlow;
layout(location = 2) in vec3 inCore;
layout(location = 3) in vec2 inShape;

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

void main()
{
	float time = pc.Eye.w;
	float d = abs(inUv.y);
	float d2 = d * d;
	float fraction = max(inShape.x, 1e-4);
	float aa = fwidth(inUv.y);
	float core = (1.0 - smoothstep(fraction - aa, fraction + aa, d)) * inShape.y;
	float halo = exp(-d2 / (fraction * fraction * 7.0)) * min(inShape.y * 1.5, 1.0);
	float glow = exp(-d2 * 5.5) * (1.0 - d2);
	float crackle = 0.65 + 0.7 * Noise(vec2(inUv.x * 7.0 - time * 41.0, time * 17.0));
	vec3 radiance = inCore * core + mix(inCore, inGlow * 2.0, 0.65) * (halo * 0.5) + inGlow * (glow * crackle);

	vec2 screen = gl_FragCoord.xy * pc.Screen.zw;
	float near = pc.Camera.x;
	float far = pc.Camera.y;
	float sceneZ = near * far / (far - textureLod(SceneDepth, screen, 0.0).r * (far - near));
	float boltZ = near * far / (far - gl_FragCoord.z * (far - near));
	radiance *= clamp((sceneZ - boltZ) / pc.Right.w, 0.0, 1.0);
	outScene = vec4(radiance, 0.0);
	outEmission = vec4(radiance * pc.Camera.w, 0.0);
}
