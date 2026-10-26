#version 450
#include "GBuffer.glsl"
#include "Material.glsl"

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 Focus;
	vec4 Eye;
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

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outSurface;
layout(location = 2) out vec4 outEmission;

const float LavaLine = -0.3;
const vec3 LavaHeat = vec3(0.7, 0.19, 0.04);
const vec3 HotRock = vec3(0.9, 0.2, 0.03);
const vec3 CrystalTeal = vec3(0.08, 0.8, 0.72);
const vec3 CrystalViolet = vec3(0.5, 0.26, 1.0);
const float CrystalGlow = 0.45;
const uint CrystalTexture = 1u;
const float CrystalPeriod = 20.0;
const float IcePeriod = 18.0;
const vec3 IceCrack = vec3(0.86, 0.95, 1.0);
const float DyeGain = 5.2;
const float DyeFloor = 0.014;
const float TrimLight = 0.3;
const float TrimGlow = 0.12;
const float RimPower = 3.5;

struct Layer
{
	vec4 Albedo;
	vec4 Surface;
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
	layer.Surface = texture(Textures[nonuniformEXT(m.Surface)], st) * vec4(1.0, m.Params.x, m.Params.y, 1.0);
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

Layer Splat(uvec2 ids, vec4 color, vec2 uv)
{
	const float depth = 0.25;
	uint materialsOf[4] = uint[](ids.x & 0xFFFFu, ids.x >> 16, ids.y & 0xFFFFu, ids.y >> 16);
	float weights[4] = float[](max(1.0 - color.r - color.g - color.b, 0.0), color.r, color.g, color.b);
	vec2 dx = dFdx(uv);
	vec2 dy = dFdy(uv);
	vec4 albedo[4];
	float scores[4];
	float top = -1e9;
	for (int i = 0; i < 4; i++)
	{
		scores[i] = -1e9;
		albedo[i] = vec4(0.0);
		if (weights[i] > 0.01)
		{
			Material m = materials[materialsOf[i]];
			float scale = m.Params.z;
			albedo[i] = textureGrad(Textures[nonuniformEXT(m.Albedo)], uv * scale, dx * scale, dy * scale);
			scores[i] = albedo[i].a + 2.0 * weights[i];
			top = max(top, scores[i]);
		}
	}
	Layer result;
	result.Albedo = vec4(0.0);
	result.Surface = vec4(0.0);
	result.Normal = vec3(0.0);
	float total = 0.0;
	for (int i = 0; i < 4; i++)
	{
		float share = max(scores[i] - top + depth, 0.0);
		if (share <= 0.0)
			continue;
		Material m = materials[materialsOf[i]];
		float scale = m.Params.z;
		vec2 st = uv * scale;
		result.Albedo += vec4(albedo[i].rgb * m.Color.rgb, albedo[i].a) * share;
		result.Surface += textureGrad(Textures[nonuniformEXT(m.Surface)], st, dx * scale, dy * scale) * vec4(1.0, m.Params.x, m.Params.y, 1.0) * share;
		vec3 normal = vec3(0.0, 0.0, 1.0);
		if (m.Normal != 0u)
		{
			vec2 xy = textureGrad(Textures[nonuniformEXT(m.Normal)], st, dx * scale, dy * scale).rg * 2.0 - 1.0;
			normal = vec3(xy, sqrt(max(1.0 - dot(xy, xy), 0.0)));
		}
		result.Normal += normal * share;
		total += share;
	}
	result.Albedo /= total;
	result.Surface /= total;
	result.Normal = normalize(result.Normal);
	return result;
}

float VeinHash(vec3 p)
{
	p = fract(p * 0.3183099 + 0.1) * 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float VeinNoise(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	vec3 u = f * f * (3.0 - 2.0 * f);
	float near = mix(mix(VeinHash(i), VeinHash(i + vec3(1.0, 0.0, 0.0)), u.x), mix(VeinHash(i + vec3(0.0, 1.0, 0.0)), VeinHash(i + vec3(1.0, 1.0, 0.0)), u.x), u.y);
	float far = mix(mix(VeinHash(i + vec3(0.0, 0.0, 1.0)), VeinHash(i + vec3(1.0, 0.0, 1.0)), u.x), mix(VeinHash(i + vec3(0.0, 1.0, 1.0)), VeinHash(i + vec3(1.0, 1.0, 1.0)), u.x), u.y);
	return mix(near, far, u.z);
}

vec3 Veins(vec3 world)
{
	float region = smoothstep(0.5, 0.78, VeinNoise(world * 0.09 + 3.7));
	float n = 0.65 * VeinNoise(world * 0.6) + 0.35 * VeinNoise(world * 1.5 + 7.1);
	float vein = smoothstep(0.955, 0.99, 1.0 - abs(2.0 * n - 1.0)) * region;
	return vec3(1.0, 0.36, 0.08) * vein * (0.2 + 1.1 * smoothstep(-2.0, -14.0, world.y));
}

#ifdef CRYSTAL
vec3 CrystalVeins(vec3 world, float field, out float vein)
{
	float n = texture(Textures[CrystalTexture], world.xz / CrystalPeriod).r;
	float distance = abs(n - 0.5) / max(fwidth(n), 1e-5);
	vein = (1.0 - smoothstep(0.4 * field, 0.4 + field, distance)) * field;
	float hue = 0.5 + 0.5 * sin(world.x * 0.23 + 1.3) * sin(world.z * 0.19 + 0.4);
	return mix(CrystalTeal, CrystalViolet, smoothstep(0.3, 0.7, hue));
}
#endif

float Luminance(vec3 color)
{
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
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
	bool splat = (flags & MeshSplat) != 0u;
	bool layered = (flags & MeshLayered) != 0u;
	bool onlyOther = layered && inColor.r > 0.995;
	Material material = materials[splat ? (inMaterial.x & 0xFFFFu) : onlyOther ? inMaterial.y : inMaterial.x];
	bool backFacing = gl_FrontFacing != (inMirrored == 0u);
	if (backFacing && (material.Flags & MaterialSingleSided) != 0u)
		discard;
	bool water = (flags & MeshWater) != 0u;
	bool lake = water && (flags & MeshGround) != 0u;
	bool veins = (flags & MeshVeins) != 0u;
	bool projected = veins || (flags & MeshCliff) != 0u;
	float time = inCut.x;
	vec2 uv = inUv;
	vec4 tangent = inTangent;
	if (projected)
	{
		vec3 facing = normalize(inNormal);
		vec3 axes = abs(facing);
		vec3 dPdu = vec3(1.0, 0.0, 0.0);
		vec3 dPdv = vec3(0.0, -1.0, 0.0);
		if (axes.y > 0.75)
		{
			uv = inWorld.xz;
			dPdv = vec3(0.0, 0.0, 1.0);
		}
		else if (axes.x > axes.z)
		{
			uv = vec2(inWorld.z, -inWorld.y);
			dPdu = vec3(0.0, 0.0, 1.0);
		}
		else
			uv = vec2(inWorld.x, -inWorld.y);
		tangent = vec4(dPdu, dot(cross(facing, dPdu), -dPdv) >= 0.0 ? 1.0 : -1.0);
	}
	if (water && !lake)
		uv += 0.004 * vec2(sin(dot(inUv, vec2(55.0, 31.0)) + time * 1.3), cos(dot(inUv, vec2(-27.0, 49.0)) + time * 1.1));
	Layer layer = splat ? Splat(inMaterial.xy, inColor, uv) : Fetch(material, uv);
	if (lake)
	{
		vec3 drift = Fetch(material, uv * 0.71 + vec2(0.023, -0.017) * time).Normal;
		vec3 chop = Fetch(material, uv * 1.37 + vec2(-0.031, 0.026) * time).Normal;
		layer.Normal = normalize(vec3(drift.xy + chop.xy, drift.z * chop.z));
	}
	if (!splat && (material.Flags & MaterialAlphaTest) != 0u && layer.Albedo.a < material.Color.a)
		discard;
	if (layered && !onlyOther && inColor.r > 0.005)
	{
		Layer other = Fetch(materials[inMaterial.y], inUv);
		float blend = HeightBlend(layer.Albedo.a, other.Albedo.a, inColor.r);
		layer.Albedo = mix(layer.Albedo, other.Albedo, blend);
		layer.Surface = mix(layer.Surface, other.Surface, blend);
		layer.Normal = normalize(mix(layer.Normal, other.Normal, blend));
	}
	if (water)
	{
		vec2 wave = inUv * (lake ? 5.0 : 90.0);
		vec2 slope = vec2(0.8, 0.6) * cos(dot(wave, vec2(0.8, 0.6)) + time * 1.9)
			+ vec2(-0.55, 0.83) * cos(dot(wave, vec2(-0.55, 0.83)) * 1.37 + time * 2.6)
			+ vec2(0.28, -0.96) * cos(dot(wave, vec2(0.28, -0.96)) * 0.71 + time * 1.4);
		layer.Normal = normalize(layer.Normal + vec3(slope * (lake ? 0.04 : 0.09), 0.0));
	}
	float height = layer.Albedo.a;
	vec3 albedo = layer.Albedo.rgb;
	if ((flags & MeshVertexTint) != 0u)
		albedo *= inColor.rgb;
	if (splat)
		albedo *= inColor.a;
	vec3 heat = veins ? Veins(inWorld) : vec3(0.0);
	if ((flags & MeshLavaGlow) != 0u)
	{
		float near = 1.0 - smoothstep(LavaLine, LavaLine + 0.4, inShading.z);
		albedo *= 1.0 - 0.55 * near;
		heat = LavaHeat * (near * near);
	}
	if (layered)
		albedo *= 0.62 + 0.76 * inColor.b;
#ifdef CRYSTAL
	float iceRoughness = -1.0;
	if ((flags & MeshIce) != 0u)
	{
		float n = texture(Textures[CrystalTexture], inWorld.xz / IcePeriod + vec2(0.31, 0.17)).r;
		float reach = texture(Textures[CrystalTexture], inWorld.xz / (IcePeriod * 2.2) + vec2(0.77, 0.41)).r;
		float crack = (1.0 - smoothstep(0.35, 1.2, abs(n - 0.5) / max(fwidth(n), 1e-5))) * smoothstep(0.42, 0.6, reach);
		crack *= smoothstep(0.6, 0.9, normalize(inNormal).y) * (1.0 - inColor.a);
		albedo = mix(albedo, IceCrack, crack * 0.7);
		iceRoughness = mix(mix(0.16, 0.35, crack), 0.85, inColor.a);
	}
	if ((flags & MeshCrystal) != 0u)
	{
		float vein;
		vec3 crystal = CrystalVeins(inWorld, inColor.a, vein);
		vein *= smoothstep(0.55, 0.85, normalize(inNormal).y);
		albedo = mix(albedo, crystal * 0.55, vein);
		heat += crystal * (vein * CrystalGlow);
	}
#endif
	float trim = 0.0;
	if ((flags & (MeshAccentTrim | MeshAccentDye)) != 0u)
	{
		float light = Luminance(albedo);
		vec3 tone = inAccent.rgb / max(Luminance(inAccent.rgb), 0.02);
		if ((flags & MeshAccentTrim) != 0u)
			trim = smoothstep(0.3, 0.55, (albedo.r - albedo.b) / max(albedo.r, 1e-4)) * smoothstep(0.05, 0.14, light);
		if ((flags & MeshAccentDye) != 0u)
			albedo = min(tone * (light * DyeGain + DyeFloor), vec3(0.95));
		albedo = mix(albedo, min(tone * TrimLight, vec3(0.95)), trim);
		heat += inAccent.rgb * (trim * TrimGlow);
	}
	float occlusion = layer.Surface.r;
	float roughness = layer.Surface.g;
	float metallic = layer.Surface.b;
#ifdef CRYSTAL
	if (iceRoughness >= 0.0)
		roughness = iceRoughness;
#endif

	vec3 geometric = normalize(inNormal);
	vec3 n = geometric;
	vec3 t = tangent.xyz - geometric * dot(geometric, tangent.xyz);
	if (layer.Normal.z < 0.9999 && dot(t, t) > 1e-10)
	{
		t = normalize(t);
		vec3 b = cross(geometric, t) * tangent.w;
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
	if (backFacing)
		n = -n;
	if (inAccent.a > 0.0)
	{
		float facing = clamp(dot(n, normalize(pc.Eye.xyz - inWorld)), 0.0, 1.0);
		heat += inAccent.rgb * (pow(1.0 - facing, RimPower) * inAccent.a);
	}
	if ((material.Flags & MaterialPbr) != 0u)
		roughness = FilterRoughness(n, roughness);
	else
		occlusion *= GroundOcclusion(inShading.y);

	albedo = mix(albedo * inTint.rgb, vec3(1.0), clamp(inTint.a, 0.0, 1.0));
	float glow = clamp(inShading.x, 0.0, 1.0);
	if ((flags & MeshLavaGlow) != 0u)
		heat += HotRock * glow;
	float fade = DepthFade(pc.Focus.x, inShading.z);
	outAlbedo = vec4(albedo * (1.0 - glow), occlusion);
	outSurface = PackSurface(n, roughness, metallic);
	outEmission = vec4((albedo * glow + heat + material.Extra.rgb * layer.Surface.a) * (1.0 - fade), cut > 0.0 ? 1.0 : 0.0);
}
