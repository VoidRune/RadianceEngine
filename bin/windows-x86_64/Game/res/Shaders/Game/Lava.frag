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

const float Period = 16.0;
const vec3 CrustColor = vec3(0.034, 0.037, 0.046);
const vec3 Glow = vec3(1.7, 0.5, 0.1);

void main()
{
	vec2 p = inWorld.xz;
	float time = inTime;
	float pixel = max(length(dFdx(p)), length(dFdy(p)));
	vec2 warp = texture(Crust, p / (Period * 1.63) + time * vec2(0.0021, 0.0016)).ab - 0.5;
	float flow = texture(Crust, p / (Period * 0.95) + warp * 0.16 + time * vec2(-0.0012, 0.0019)).b;
	vec2 cells = texture(Crust, p / Period + warp * 0.035).rg;
	float open = 1.0 - smoothstep(0.22, 0.26, flow);
	float near = 1.0 - smoothstep(0.22, 0.42, flow);
	float border = cells.x;
	float width = mix(0.02, 0.075, near);
	float crack = 1.0 - smoothstep(width - pixel * 0.5, width + pixel * 0.5, border);
	float pulse = 0.8 + 0.2 * sin(time * 1.1 + cells.y * 6.2832 + flow * 7.0);
	float crust = (0.0015 + 0.003 * cells.y + 0.1 * near * near) * (1.0 - crack) + exp(-border * 15.0) * (0.014 + 0.08 * near) + crack * (0.28 + 0.2 * near) * pulse;
	float river = 0.26 + 0.36 * (1.0 - smoothstep(0.04, 0.2, flow)) + 0.05 * sin(time * 0.9 + flow * 23.0);
	float coverage = (1.0 - open) * (1.0 - crack) * (0.8 + 0.2 * cells.y);
	float fade = DepthFade(pc.Params.x, inWorld.y);
	outAlbedo = vec4(CrustColor * coverage, GroundOcclusion(0.0));
	outSurface = PackSurface(vec3(0.0, 1.0, 0.0), 0.85, 0.0);
	outEmission = vec4(Glow * (mix(crust, river, open) * (1.0 - fade)), 0.0);
}
