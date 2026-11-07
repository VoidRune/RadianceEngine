#version 450
#include "GBuffer.glsl"
#include "Lake.glsl"

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

const vec3 Deep = vec3(0.002, 0.006, 0.009);

vec2 Ripple(vec2 p, vec2 flow, float time)
{
	return texture(Crust, p + flow * time).ab - 0.5;
}

void main()
{
	vec2 p = inWorld.xz;
	vec2 turned = vec2(p.x * 0.8 - p.y * 0.6, p.x * 0.6 + p.y * 0.8);
	vec2 skew = vec2(p.x * 0.28 + p.y * 0.96, p.y * 0.28 - p.x * 0.96);
	float gust = smoothstep(-0.28, 0.3, Ripple(p / 61.0, vec2(0.0023, 0.0011), inTime).x + 0.5 * Ripple(skew / 23.0, vec2(-0.0031, 0.0026), inTime).y);
	vec2 slope = Ripple(turned / 7.3, vec2(0.009, 0.004), inTime) * 0.05;
	slope += Ripple(skew / 3.1, vec2(-0.018, 0.012), inTime) * 0.09;
	slope += Ripple(p / 1.37, vec2(0.031, -0.027), inTime) * 0.13;
	slope *= 0.28 + 0.72 * gust;
	outAlbedo = vec4(Deep, 1.0);
	outSurface = PackSurface(normalize(vec3(slope.x, 1.0, slope.y)), 0.07, 0.0);
	outEmission = vec4(vec3(0.0), TagWater);
}
