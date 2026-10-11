#version 450
#include "Particles.glsl"
#include "EffectShapes.glsl"

layout(std430, set = 0, binding = 4) readonly buffer FrameBuffer { ParticleFrame Frame; };
layout(set = 0, binding = 5) uniform sampler2D SceneDepth;
layout(std430, set = 0, binding = 6) readonly buffer DecalBuffer { DecalInstance Decals[]; };

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
	if (max(abs(uv.x), abs(uv.y)) > 1.0 || height > 1.0)
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
	if (shape == DecalRunes)
		result.y = RuneCircle(uv, pixel, extent.x, params.x, params.y, params.z, params.w, seed, time);
	else if (shape == DecalSector)
		result.y = SectorShape(uv, pixel, extent.x, params.z, params.x, params.y);
	else if (shape == DecalLane)
		result.y = LaneShape(uv, extent.xy, pixelWorld, params.x, params.y, time);
	else if (shape == DecalRing)
		result.y = RingShape(uv, pixel, extent.x, params.x, params.w, params.y, params.z, time);
	else if (shape == DecalGlow)
	{
		float r2 = dot(uv, uv);
		result.y = exp(-r2 * 3.5) * (1.0 - smoothstep(0.7, 1.0, sqrt(r2)));
	}
	else if (shape == DecalScorch)
		result = ScorchShape(uv, params.x, seed);
	else if (shape == DecalFrost)
		result = FrostShape(uv, params.x, seed, time);
	else if (shape == DecalCracks)
		result = CracksShape(uv, pixel, extent.x, params.x, seed);
	float coverage = clamp(result.x * decal.Tint.a, 0.0, 1.0) * mask;
	float roughness = decal.Extent.w;
	float surfaceCoverage = roughness >= 0.0 ? coverage : 0.0;
	outAlbedo = vec4(decal.Tint.rgb * coverage, coverage);
	outSurface = vec4(0.0, 0.0, roughness * surfaceCoverage, surfaceCoverage);
	outEmission = vec4(decal.Color.rgb * (max(result.y, 0.0) * decal.Color.a * mask), 0.0);
}
