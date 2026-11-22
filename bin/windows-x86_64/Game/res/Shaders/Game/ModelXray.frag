#version 450

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 Focus;
	vec4 Eye;
	vec4 Sight;
} pc;

layout(location = 0) in vec3 inNormal;
layout(location = 2) flat in uvec3 inMaterial;
layout(location = 9) in vec3 inWorld;
layout(location = 10) flat in vec4 inAccent;

layout(location = 0) out vec4 outMask;

const uint MeshHero = 134217728u;
const vec3 FallbackTint = vec3(0.55, 0.75, 1.0);
const float Coverage = 0.2;

void main()
{
	if ((inMaterial.z & MeshHero) == 0u)
		discard;
	vec3 toEye = normalize(pc.Eye.xyz - inWorld);
	float rim = pow(1.0 - abs(dot(normalize(inNormal), toEye)), 2.0);
	vec3 tint = dot(inAccent.rgb, vec3(1.0)) > 0.01 ? inAccent.rgb / max(max(inAccent.r, max(inAccent.g, inAccent.b)), 1e-3) : FallbackTint;
	outMask = vec4(tint, Coverage + (1.0 - Coverage) * rim);
}
