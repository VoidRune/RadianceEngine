#version 450
#include "GBuffer.glsl"

layout(set = 0, binding = 1) uniform sampler2D Crust;

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 Params;
} pc;

layout(location = 0) in vec3 inWorld;
layout(location = 1) flat in float inTime;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outSurface;
layout(location = 2) out vec4 outEmission;

const float Period = 11.0;
const float BakedPeriod = 16.0;
const vec3 Deep = vec3(0.006, 0.02, 0.05);
const vec3 Shallow = vec3(0.016, 0.06, 0.11);
const vec3 Slush = vec3(0.5, 0.66, 0.76);
const vec3 Chill = vec3(0.05, 0.16, 0.26);

void main()
{
	vec2 p = inWorld.xz;
	float time = inTime;
	float pixel = max(length(dFdx(p)), length(dFdy(p)));
	vec2 warp = texture(Crust, p / (Period * 1.9) + time * vec2(0.0011, 0.0008)).ab - 0.5;
	vec2 drift = time * vec2(0.045, 0.018) + warp * 1.6;
	vec2 cells = texture(Crust, (p - drift) / Period).rg;
	float border = cells.x * (Period / BakedPeriod);
	float density = texture(Crust, (p - drift * 0.5) / (Period * 2.4) + vec2(0.37, 0.11)).b;
	float present = step(0.52, fract(cells.y * 7.13)) * smoothstep(0.4, 0.66, density);
	float inset = 0.15 + 0.14 * fract(cells.y * 13.7) + 0.04 * (fract(cells.y * 31.3) - 0.5);
	float floe = smoothstep(inset - pixel - 0.02, inset + pixel + 0.02, border) * present;
	float rim = (1.0 - smoothstep(inset, inset + 0.08 + pixel, border)) * smoothstep(inset - 0.06 - pixel, inset, border) * present;
	vec2 slope = 0.035 * vec2(sin(p.x * 2.1 + p.y * 0.7 + time * 1.3) + 0.6 * sin(p.x * 3.7 - p.y * 2.3 - time * 1.7), cos(p.y * 1.9 - p.x * 0.6 + time * 1.1) + 0.6 * cos(p.y * 3.1 + p.x * 2.6 + time * 1.5)) + warp * 0.1;
	vec3 normal = normalize(vec3(slope.x, 1.0, slope.y));
	vec3 water = mix(Deep, Shallow, clamp(warp.x * 1.4 + 0.5, 0.0, 1.0));
	vec3 albedo = mix(water, Slush * (0.55 + 0.45 * fract(cells.y * 3.1)), floe);
	float roughness = mix(0.05, 0.8, floe);
	float fade = DepthFade(pc.Params.x, inWorld.y);
	outAlbedo = vec4(albedo, GroundOcclusion(0.0));
	outSurface = PackSurface(normalize(mix(normal, vec3(0.0, 1.0, 0.0), floe)), roughness, 0.0);
	outEmission = vec4(Chill * (0.012 + 0.05 * rim) * (1.0 - fade), 0.0);
}
