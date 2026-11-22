#version 450
#include "Material.glsl"
#include "Sight.glsl"

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 Focus;
	vec4 Eye;
	vec4 Sight;
} pc;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUv;
layout(location = 2) flat in uvec3 inMaterial;
layout(location = 3) in vec4 inTint;
layout(location = 4) in vec3 inShading;
layout(location = 5) flat in vec2 inCut;
layout(location = 6) flat in uint inMirrored;
layout(location = 7) in vec4 inTangent;
layout(location = 8) in vec4 inColor;
layout(location = 9) in vec3 inWorld;

void main()
{
	if (gl_FrontFacing == (inMirrored == 0u))
		discard;
	if (inCut.y > 0.0 && SightDistance(inWorld, pc.Eye.xyz, pc.Sight) < 1.0)
		discard;
	Material material = materials[inMaterial.x];
	if ((material.Flags & MaterialAlphaTest) != 0u && SampleAlbedo(material, inUv).a < material.Color.a)
		discard;
}
