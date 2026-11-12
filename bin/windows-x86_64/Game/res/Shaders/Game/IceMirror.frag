#version 450
#include "Material.glsl"

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 Focus;
	vec4 SunDirection;
	vec4 SunRadiance;
	vec4 SkyRadiance;
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
layout(location = 10) flat in vec4 inAccent;
layout(location = 11) flat in uint inFlash;

layout(location = 0) out vec4 outColor;

void main()
{
	if (inWorld.y < pc.SunDirection.w)
		discard;
	Material material = materials[inMaterial.x];
	vec4 sampled = texture(Textures[nonuniformEXT(material.Albedo)], inUv * material.Params.z);
	if ((material.Flags & MaterialAlphaTest) != 0u && sampled.a < material.Color.a)
		discard;
	vec3 albedo = sampled.rgb * material.Color.rgb;
	if ((inMaterial.z & MeshVertexTint) != 0u)
		albedo *= inColor.rgb;
	vec3 n = normalize(inNormal);
	vec3 tint = inTint.rgb;
	if ((inMaterial.z & MeshAccentDye) != 0u)
		tint = mix(tint, inAccent.rgb, (material.Flags & MaterialAlphaTest) != 0u ? 0.5 : 0.5 * sampled.a);
	albedo *= tint;
	vec3 lit = albedo * (pc.SunRadiance.rgb * max(dot(n, pc.SunDirection.xyz), 0.0) * 0.318 + pc.SkyRadiance.rgb * (0.65 + 0.35 * n.y));
	vec4 surface = texture(Textures[nonuniformEXT(material.Surface)], inUv * material.Params.z);
	vec3 emitted = material.Extra.rgb * surface.a + albedo * clamp(inShading.x, 0.0, 1.0);
	if ((inMaterial.z & MeshBright) != 0u)
		emitted *= 3.0;
	outColor = vec4(lit + emitted, 1.0);
}
