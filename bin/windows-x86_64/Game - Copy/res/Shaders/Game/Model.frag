#version 450
#extension GL_EXT_nonuniform_qualifier : require
#include "GBuffer.glsl"

layout(set = 0, binding = 3) uniform sampler2D Textures[14];
layout(set = 0, binding = 4) uniform sampler2D Surfaces[14];

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 Focus;
} pc;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUv;
layout(location = 2) flat in uvec2 inTextures;
layout(location = 3) in vec4 inTint;
layout(location = 4) in vec3 inShading;
layout(location = 5) flat in vec2 inCut;
layout(location = 6) flat in vec2 inMaterial;
layout(location = 7) flat in uint inMirrored;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outSurface;
layout(location = 2) out vec4 outEmission;

float Bayer4(uvec2 pixel)
{
	const float matrix[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
	return (matrix[(pixel.y & 3u) * 4u + (pixel.x & 3u)] + 0.5) / 16.0;
}

void main()
{
	float cut = inCut.y > 0.0 ? inCut.y * clamp((inShading.z - inCut.x) / 0.35, 0.0, 1.0) : 0.0;
	if (Bayer4(uvec2(gl_FragCoord.xy) + uvec2(4.0 - pc.Focus.zw)) < cut)
		discard;
	vec3 albedo = mix(texture(Textures[nonuniformEXT(inTextures.x)], inUv).rgb * inTint.rgb, vec3(1.0), clamp(inTint.a, 0.0, 1.0));
	vec4 surface = texture(Surfaces[nonuniformEXT(inTextures.y)], inUv);
	float glow = clamp(inShading.x, 0.0, 1.0);
	float fade = DepthFade(pc.Focus.x, inShading.z);
	outAlbedo = vec4(albedo * (1.0 - glow), GroundOcclusion(inShading.y));
	outSurface = PackSurface(normalize(gl_FrontFacing == (inMirrored == 0u) ? inNormal : -inNormal), inMaterial.x * surface.g, inMaterial.y * surface.b);
	outEmission = vec4(albedo * (glow * (1.0 - fade)), cut > 0.0 ? 1.0 : 0.0);
}
