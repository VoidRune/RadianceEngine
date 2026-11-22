#version 450
#include "GBuffer.glsl"
#include "Sight.glsl"

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 Focus;
	vec4 Eye;
	vec4 Sight;
} pc;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inHeight;
layout(location = 3) in float inWorldHeight;
layout(location = 4) flat in vec3 inSurface;
layout(location = 5) in vec3 inWorld;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outSurface;
layout(location = 2) out vec4 outEmission;

void main()
{
	float sight = inSurface.z > 0.5 ? SightDistance(inWorld, pc.Eye.xyz, pc.Sight) : 2.0;
	if (sight < 1.0)
		discard;
	float glow = clamp(inColor.a, 0.0, 1.0);
	float fade = DepthFade(pc.Focus.x, inWorldHeight);
	outAlbedo = vec4(inColor.rgb * (1.0 - glow), GroundOcclusion(inHeight));
	outSurface = PackSurface(normalize(gl_FrontFacing ? inNormal : -inNormal), inSurface.x, inSurface.y);
	outEmission = vec4((inColor.rgb * glow + SightGlow * SightRimGlow(sight)) * (1.0 - fade), 0.0);
}
