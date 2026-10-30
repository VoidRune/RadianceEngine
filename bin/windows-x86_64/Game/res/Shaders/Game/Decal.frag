#version 450
#include "Particles.glsl"
#include "EffectShapes.glsl"

layout(std430, set = 0, binding = 4) readonly buffer FrameBuffer { ParticleFrame Frame; };
layout(set = 0, binding = 5) uniform sampler2D SceneDepth;
layout(std430, set = 0, binding = 6) readonly buffer DecalBuffer { DecalInstance Decals[]; };
layout(set = 0, binding = 7) uniform sampler2D CausticMap;

layout(location = 0) flat in uint inIndex;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outSurface;
layout(location = 2) out vec4 outEmission;

vec3 WorldAt(vec2 screen)
{
	vec4 world = Frame.InverseViewProjection * vec4(screen * 2.0 - 1.0, textureLod(SceneDepth, screen, 0.0).r, 1.0);
	return world.xyz / world.w;
}

vec3 NearerDelta(vec2 screen, vec2 offset, vec3 center)
{
	vec3 forward = WorldAt(screen + offset) - center;
	vec3 backward = center - WorldAt(screen - offset);
	return dot(forward, forward) <= dot(backward, backward) ? forward : backward;
}

vec3 CausticShape(vec2 uv, float radius, float pixel, float spin, float spread, float broken, uint seed, float time)
{
	float r = length(uv);
	float window = 1.0 - smoothstep(0.3, 0.95, r);
	if (window <= 0.0)
		return vec3(0.0);
	const float sector = Tau / 8.0;
	float turned = atan(uv.y, uv.x) - spin;
	float angle = abs(mod(turned, sector) - 0.5 * sector);
	vec2 folded = vec2(cos(angle), sin(angle)) * (r * radius);
	float cells = mix(3.0, 4.2, broken);
	float width = pixel * radius * cells * 1.5;
	vec2 key = vec2(float(seed & 1023u), float((seed >> 10) & 1023u)) / 1023.0;
	vec2 driftA = vec2(0.11, 0.07) * time + key;
	vec2 driftB = vec2(-0.08, 0.13) * time + key.yx;
	float rays = pow(0.5 + 0.5 * cos(turned * 8.0), 4.0);
	vec2 base = folded * (cells / 8.0);
	float glints = 0.25 + 0.75 * exp(-textureLod(CausticMap, base * 1.7 + driftB, 0.0).r / (0.06 + width));
	vec3 light = vec3(0.0);
	for (int c = 0; c < 3; c++)
	{
		float stretch = 1.0 + (float(c) - 1.0) * spread;
		float lines = exp(-textureLod(CausticMap, base * stretch + driftA, 0.0).r / (0.07 + width));
		light[c] = lines * glints * 0.9 + rays * 0.35 * (1.0 - smoothstep(0.1, 0.8, r * stretch));
	}
	return light * (window * (0.35 + 0.65 * exp(-r * r * 6.0)));
}

void main()
{
	outAlbedo = vec4(0.0);
	outSurface = vec4(0.0);
	outEmission = vec4(0.0);
	DecalInstance decal = Decals[inIndex];
	vec2 texel = 1.0 / Frame.Screen.xy;
	vec2 screen = gl_FragCoord.xy * texel;
	float depth = textureLod(SceneDepth, screen, 0.0).r;
	if (depth >= 1.0)
		return;
	vec4 world4 = Frame.InverseViewProjection * vec4(screen * 2.0 - 1.0, depth, 1.0);
	vec3 world = world4.xyz / world4.w;
	vec3 offset = world - decal.CenterYaw.xyz;
	float c = cos(decal.CenterYaw.w);
	float s = sin(decal.CenterYaw.w);
	vec3 extent = decal.Extent.xyz;
	vec2 uv = vec2(offset.x * c + offset.z * s, offset.z * c - offset.x * s) / extent.xy;
	float height = abs(offset.y) / extent.z;
	if (max(abs(uv.x), abs(uv.y)) > 1.0 || height > 1.0 || (decal.Info.x == DecalCaustic && dot(uv, uv) > 0.92))
		return;
	vec3 normal = cross(NearerDelta(screen, vec2(0.0, texel.y), world), NearerDelta(screen, vec2(texel.x, 0.0), world));
	float facing = abs(normal.y) * inversesqrt(max(dot(normal, normal), 1e-12));
	float mask = smoothstep(0.55, 0.8, facing) * (1.0 - smoothstep(0.6, 1.0, height));
	float near = Frame.CameraForward.w;
	float far = Frame.Screen.z;
	float pixelWorld = near * far / (far - depth * (far - near)) * Frame.Screen.w * 1.3;
	float pixel = pixelWorld / extent.x;
	float time = Frame.CameraRight.w;
	vec4 params = decal.Params;
	uint shape = decal.Info.x;
	uint seed = decal.Info.y;
	vec2 result = vec2(0.0);
	vec3 glow = vec3(1.0);
	vec3 albedo = decal.Tint.rgb;
	float roughness = decal.Extent.w;
	if (shape == DecalRunes)
		result.y = RuneCircle(uv, pixel, extent.x, params.x, params.y, params.z, params.w, seed, time);
	else if (shape == DecalSector)
		result.y = SectorShape(uv, pixel, extent.x, params.z, params.x, params.y);
	else if (shape == DecalLane)
		result.y = LaneShape(uv, extent.xy, pixelWorld, params.x, params.y, time);
	else if (shape == DecalRing)
		result.y = RingShape(uv, pixel, extent.x, params.x, params.w, params.y, params.z, time, seed);
	else if (shape == DecalGlow)
	{
		float r2 = dot(uv, uv);
		result.y = exp(-r2 * 3.5) * (1.0 - smoothstep(0.7, 1.0, sqrt(r2)));
	}
	else if (shape == DecalScorch)
		result = ScorchShape(uv, params.x, seed);
	else if (shape == DecalFrost || shape == DecalFrostLane)
	{
		Rime rime = shape == DecalFrost ? FrostShape(uv, extent.x, pixelWorld, params.x, seed, time) : FrostLaneShape(uv, extent.xy, pixelWorld, params.x, seed, time);
		result = vec2(rime.Coverage, rime.Glow);
		albedo *= rime.Shade;
		if (roughness >= 0.0)
			roughness = rime.Roughness;
	}
	else if (shape == DecalCracks)
		result = CracksShape(uv, pixel, extent.x, params.x, seed);
	else if (shape == DecalCharge)
		result = ChargeShape(uv, pixel, params.x, seed, time);
	else if (shape == DecalFracture)
		result = FractureShape(world.xz, pixelWorld, params.x, time);
	else if (shape == DecalFissure)
		result = FissureShape(uv, extent.xy, pixelWorld, params.x, params.y, params.z, seed, time);
	else if (shape == DecalCaustic)
	{
		glow = CausticShape(uv, extent.x, pixel, params.x, params.y, params.z, seed, time);
		result = vec2(1.0 - smoothstep(0.3, 0.9, length(uv)), 1.0);
	}
	float coverage = clamp(result.x * decal.Tint.a, 0.0, 1.0) * mask;
	float surfaceCoverage = roughness >= 0.0 ? coverage : 0.0;
	outAlbedo = vec4(albedo * coverage, coverage);
	outSurface = vec4(0.0, 0.0, roughness * surfaceCoverage, surfaceCoverage);
	outEmission = vec4(decal.Color.rgb * glow * (max(result.y, 0.0) * decal.Color.a * mask), 0.0);
}
