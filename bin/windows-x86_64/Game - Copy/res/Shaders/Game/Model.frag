#version 450
#include "GBuffer.glsl"
#include "Material.glsl"

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 Focus;
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

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outSurface;
layout(location = 2) out vec4 outEmission;

struct Layer
{
	vec4 Albedo;
	vec3 Surface;
	vec3 Normal;
};

float Bayer4(uvec2 pixel)
{
	const float matrix[16] = float[](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
	return (matrix[(pixel.y & 3u) * 4u + (pixel.x & 3u)] + 0.5) / 16.0;
}

Layer Fetch(Material m, vec2 uv)
{
	Layer layer;
	vec2 st = uv * m.Params.z;
	layer.Albedo = texture(Textures[nonuniformEXT(m.Albedo)], st);
	layer.Albedo.rgb *= m.Color.rgb;
	layer.Surface = texture(Textures[nonuniformEXT(m.Surface)], st).rgb * vec3(1.0, m.Params.x, m.Params.y);
	layer.Normal = vec3(0.0, 0.0, 1.0);
	if (m.Normal != 0u)
	{
		vec2 xy = texture(Textures[nonuniformEXT(m.Normal)], st).rg * 2.0 - 1.0;
		layer.Normal = vec3(xy, sqrt(max(1.0 - dot(xy, xy), 0.0)));
	}
	return layer;
}

float HeightBlend(float heightA, float heightB, float weight)
{
	const float depth = 0.2;
	float a = heightA + 1.0 - weight;
	float b = heightB + weight;
	float top = max(a, b) - depth;
	float wa = max(a - top, 0.0);
	float wb = max(b - top, 0.0);
	return wb / (wa + wb);
}

float FilterRoughness(vec3 n, float roughness)
{
	vec3 dx = dFdx(n);
	vec3 dy = dFdy(n);
	float kernel = min(0.5 * (dot(dx, dx) + dot(dy, dy)), 0.18);
	float alpha2 = roughness * roughness * roughness * roughness;
	return sqrt(sqrt(clamp(alpha2 + kernel, 0.0, 1.0)));
}

void main()
{
	float cut = inCut.y > 0.0 ? inCut.y * clamp((inShading.z - inCut.x) / 0.35, 0.0, 1.0) : 0.0;
	if (Bayer4(uvec2(gl_FragCoord.xy) + uvec2(4.0 - pc.Focus.zw)) < cut)
		discard;
	uint flags = inMaterial.z;
	bool layered = (flags & MeshLayered) != 0u;
	bool onlyOther = layered && inColor.r > 0.995;
	Material material = materials[onlyOther ? inMaterial.y : inMaterial.x];
	Layer layer = Fetch(material, inUv);
	if ((material.Flags & MaterialAlphaTest) != 0u && layer.Albedo.a < material.Color.a)
		discard;
	if (layered && !onlyOther && inColor.r > 0.005)
	{
		Layer other = Fetch(materials[inMaterial.y], inUv);
		float blend = HeightBlend(layer.Albedo.a, other.Albedo.a, inColor.r);
		layer.Albedo = mix(layer.Albedo, other.Albedo, blend);
		layer.Surface = mix(layer.Surface, other.Surface, blend);
		layer.Normal = normalize(mix(layer.Normal, other.Normal, blend));
	}
	float height = layer.Albedo.a;
	vec3 albedo = layer.Albedo.rgb;
	if ((flags & MeshVertexTint) != 0u)
		albedo *= inColor.rgb;
	if (layered)
		albedo *= 0.62 + 0.76 * inColor.b;
	float occlusion = layer.Surface.r;
	float roughness = layer.Surface.g;
	float metallic = layer.Surface.b;

	vec3 geometric = normalize(inNormal);
	vec3 n = geometric;
	vec3 t = inTangent.xyz - geometric * dot(geometric, inTangent.xyz);
	if (layer.Normal.z < 0.9999 && dot(t, t) > 1e-10)
	{
		t = normalize(t);
		vec3 b = cross(geometric, t) * inTangent.w;
		n = normalize(t * layer.Normal.x + b * layer.Normal.y + geometric * layer.Normal.z);
	}
	if ((flags & MeshWet) != 0u)
	{
		float wet = inColor.g;
		float surfaceWet = clamp(wet * 2.5, 0.0, 1.0);
		float puddle = smoothstep(-0.04, 0.04, wet * 1.15 - 0.17 - height);
		float porosity = clamp((roughness - 0.25) * 1.6, 0.0, 1.0) * (1.0 - metallic);
		albedo *= mix(1.0, 0.5, surfaceWet * porosity);
		roughness = mix(roughness, roughness * 0.35, surfaceWet);
		albedo *= mix(1.0, 0.75, puddle);
		roughness = mix(roughness, 0.03, puddle);
		metallic *= 1.0 - puddle;
		n = normalize(mix(n, geometric, puddle));
	}
	if (gl_FrontFacing != (inMirrored == 0u))
		n = -n;
	if ((material.Flags & MaterialPbr) != 0u)
		roughness = FilterRoughness(n, roughness);
	else
		occlusion *= GroundOcclusion(inShading.y);

	albedo = mix(albedo * inTint.rgb, vec3(1.0), clamp(inTint.a, 0.0, 1.0));
	float glow = clamp(inShading.x, 0.0, 1.0);
	float fade = DepthFade(pc.Focus.x, inShading.z);
	outAlbedo = vec4(albedo * (1.0 - glow), occlusion);
	outSurface = PackSurface(n, roughness, metallic);
	outEmission = vec4(albedo * (glow * (1.0 - fade)), cut > 0.0 ? 1.0 : 0.0);
}
