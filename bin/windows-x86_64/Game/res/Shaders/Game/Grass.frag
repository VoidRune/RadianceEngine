#version 450
#include "GBuffer.glsl"
#include "Grass.glsl"

layout(std430, set = 0, binding = 0) readonly buffer FrameBuffer { GrassFrame Frame; };

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inWorld;
layout(location = 2) in vec2 inBlade;
layout(location = 3) flat in vec4 inTone;
layout(location = 4) flat in vec4 inEffects;
layout(location = 5) flat in uint inKind;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outSurface;
layout(location = 2) out vec4 outEmission;

const vec3 Charcoal = vec3(0.018, 0.016, 0.014);
const vec3 Rime = vec3(0.6, 0.68, 0.78);
const vec3 Ember = vec3(2.4, 0.75, 0.16);
const vec3 Petals[5] = vec3[](vec3(0.72, 0.72, 0.66), vec3(0.78, 0.55, 0.06), vec3(0.34, 0.15, 0.58), vec3(0.24, 0.38, 0.78), vec3(0.78, 0.3, 0.46));
const vec3 CloverLeaf = vec3(0.036, 0.08, 0.038);
const vec3 CloverMark = vec3(0.12, 0.16, 0.1);

void main()
{
	vec3 view = normalize(Frame.Eye.xyz - inWorld);
	vec3 n = normalize(inNormal);
	if (dot(n, view) < 0.0)
		n = -n;
	n = normalize(mix(n, vec3(0.0, 1.0, 0.0), Frame.Look.x));

	float shade = inTone.x;
	float clump = inTone.z;
	float t = clamp(inBlade.x, 0.0, 1.0);
	vec3 albedo;
	float occlusion;
	float roughness = Frame.Tip.w;
	float translucency = Frame.Root.w;
	if (inBlade.x >= 3.0)
	{
		float r = inBlade.x - 3.0;
		float band = smoothstep(0.32, 0.42, r) * (1.0 - smoothstep(0.5, 0.62, r));
		albedo = mix(CloverLeaf * (0.85 + 0.3 * clump), CloverMark, 0.55 * band) * shade;
		t = 0.5;
		occlusion = 0.75;
		translucency = 0.45;
	}
	else if (inBlade.x >= 2.0)
	{
		float r = inBlade.x - 2.0;
		uint colour = min(uint(inTone.y * 7.0 + 0.5), 4u);
		vec3 heart = colour == 1u ? vec3(0.55, 0.22, 0.03) : vec3(0.85, 0.6, 0.08);
		albedo = mix(heart, Petals[colour], smoothstep(0.18, 0.38, r)) * (0.9 + 0.2 * clump);
		t = 1.0;
		occlusion = 1.0;
		roughness = 0.55;
		translucency = 0.35;
	}
	else
	{
		float dry = inKind == GrassFlower ? 0.0 : clamp(inTone.y + 0.4 * (1.0 - inTone.w), 0.0, 1.0);
		vec3 tip = mix(Frame.Tip.rgb, Frame.Dry.rgb, dry) * (0.82 + 0.36 * clump);
		vec3 root = Frame.Root.rgb * (0.9 + 0.2 * clump);
		albedo = mix(root, tip, smoothstep(0.0, 1.0, pow(t, 0.75)));
		albedo *= shade * mix(0.55, 1.0, smoothstep(0.0, 0.55, t));
		occlusion = mix(Frame.Look.y, 1.0, smoothstep(0.0, 0.9, t));
	}

	float charred = inEffects.x;
	float heat = inEffects.y;
	float frost = inEffects.z;
	albedo = mix(albedo, Charcoal, charred * mix(0.55, 1.0, t));
	albedo = mix(albedo, Rime, frost * (0.25 + 0.65 * smoothstep(0.15, 1.0, t)));
	roughness = mix(roughness, 0.3, frost);
	vec3 emission = Ember * (heat * smoothstep(0.3, 1.0, t) * (0.55 + 0.45 * GrassNoise(inWorld.xz * 6.0 + Frame.Eye.w * 2.5)));

	outAlbedo = vec4(albedo, occlusion);
	outSurface = PackSurface(n, roughness, 0.0);
	outEmission = vec4(emission, PackEmissionTag(false, translucency * (1.0 - charred)));
}
