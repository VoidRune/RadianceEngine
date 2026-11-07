#version 450
#include "GBuffer.glsl"
#include "Material.glsl"
#ifdef DISSOLVE
#include "Dissolve.glsl"
#endif

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
layout(location = 11) flat in uint inFlash;
#ifdef DISSOLVE
layout(location = 12) in vec3 inLocal;
layout(location = 13) flat in vec4 inDissolve;
layout(location = 14) flat in vec4 inBurn;
#endif

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
const float DyeGain = 5.2;
const float DyeFloor = 0.014;
const float TrimLight = 0.3;
const float TrimGlow = 0.12;
const float RimPower = 3.5;
const float CrystalTranslucency = 0.85;
const float CharDepth = 3.0;
const float TwoToneLow = 0.1;
const float TwoToneHigh = 0.25;
const float TwoToneFacing = 0.35;
const float TwoToneUp = 0.5;
const float TwoToneVeins = 1.2;
const float BrightGain = 3.0;
const vec3 SeamHeat = vec3(2.3, 0.72, 0.13);
const float SeamBase = 0.7;
const float SeamPulse = 0.45;
const float SeamWarn = 5.0;
const vec3 ObsidianTint = vec3(0.012, 0.011, 0.017);
const float ObsidianRoughness = 0.07;
const float RockRelief = 2.4;
const vec3 GlassSky = vec3(0.09, 0.09, 0.2);
const vec3 GlassHorizon = vec3(0.24, 0.1, 0.05);
const vec3 GlassLava = vec3(0.3, 0.1, 0.03);
const vec3 PolishHorizon = vec3(0.95, 0.42, 0.14);
const vec3 PolishSky = vec3(0.12, 0.09, 0.16);
const float PolishBase = 0.35;
const float PolishFresnel = 0.9;
const vec3 PolishGlint = vec3(1.4, 0.9, 0.5);
const float GlassBase = 0.05;
const float GlassFresnel = 0.7;
const float GlassGlint = 1.1;
const vec3 MarbleVein = vec3(0.05, 0.045, 0.065);
const vec3 FlowDeep = vec3(1.2, 0.3, 0.05);
const vec3 FlowHot = vec3(1.1, 0.62, 0.13);
const vec3 MetalDeep = vec3(1.1, 0.34, 0.04);
const vec3 MetalBright = vec3(2.1, 1.2, 0.26);
const vec3 MetalWhite = vec3(3.6, 2.6, 1.1);
const vec3 MetalLiquidAlbedo = vec3(0.85, 0.55, 0.22);
const float MetalCoolSpan = 1.5;
const float FloodGain = 0.5;
const float SmoulderBlend = 2.0;
const float SmoulderFlow = 0.35;
const vec3 SmoulderDeep = vec3(0.55, 0.09, 0.015);
const vec3 SmoulderBright = vec3(1.3, 0.42, 0.06);
const float SkinThreshold = 0.5;
const float SkinRim = 0.015;
const float RaftContrast = 2.2;
const float SkinRimGlow = 1.4;
const float FissureGlow = 0.45;
const float SkinUnderglow = 0.08;
const vec3 SkinAlbedo = vec3(0.07, 0.05, 0.04);
const vec3 HeatGlow = vec3(1.5, 0.3, 0.05);
const vec2 HeatCenter = vec2(24.0, 24.0);
const float HeatSeconds = 5.0;
const float PreHeat = 0.22;
const float PreHeatSeconds = 3.0;
const float ForgeHiltLine = 6.2;
const float FloodFrontAge = 0.15;
const float FloodAgeRate = 2.0;
const float MeltRadius = 1.3;
const float MeltFloor = -0.88;
const float MeltMargin = 0.06;
const float MeltRim = 1.1;
const float MeltSpoutReach = 1.95;
const float MeltSpoutHalf = 0.32;
const float MeltSpoutTop = 1.12;
const float MeltSpoutDepth = 0.07;
const float MeltAge = 3.0;

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

float RainNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = p - i;
	f = f * f * (3.0 - 2.0 * f);
	vec4 h = fract(sin(vec4(dot(i, vec2(127.1, 311.7)), dot(i + vec2(1.0, 0.0), vec2(127.1, 311.7)), dot(i + vec2(0.0, 1.0), vec2(127.1, 311.7)), dot(i + vec2(1.0, 1.0), vec2(127.1, 311.7)))) * 43758.547);
	return mix(mix(h.x, h.y, f.x), mix(h.z, h.w, f.x), f.y);
}

float CrucibleInnerRadius(float y)
{
	if (y < -0.63)
		return mix(1.036, 1.204, clamp((y + 0.896) / 0.266, 0.0, 1.0));
	if (y < 0.14)
		return mix(1.204, 1.302, (y + 0.63) / 0.77);
	return mix(1.302, 1.372, clamp((y - 0.14) / 0.924, 0.0, 1.0));
}

vec2 CrackCells(vec2 p)
{
	vec2 cell = floor(p);
	vec2 local = p - cell;
	float first = 8.0;
	float second = 8.0;
	for (int y = -1; y <= 1; y++)
		for (int x = -1; x <= 1; x++)
		{
			vec2 offset = vec2(float(x), float(y));
			vec2 seed = fract(sin(vec2(dot(cell + offset, vec2(127.1, 311.7)), dot(cell + offset, vec2(269.5, 183.3)))) * 43758.547);
			float d = length(offset + seed - local);
			if (d < first)
			{
				second = first;
				first = d;
			}
			else if (d < second)
				second = d;
		}
	return vec2(first, second);
}

float MetalFbm(vec3 p)
{
	return VeinNoise(p) * 0.55 + VeinNoise(p * 2.07 + 13.1) * 0.3 + VeinNoise(p * 4.13 + 29.7) * 0.15;
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
#ifdef DISSOLVE
	float field = DissolveField(inLocal, inDissolve.z, inDissolve.w, inBurn.w);
	float threshold = DissolveThreshold(inDissolve.x, inDissolve.y);
	if (field <= threshold)
		discard;
	float burn = DissolveEdge(field, threshold, inDissolve.y);
	float charred = DissolveEdge(field, threshold, inDissolve.y * CharDepth);
#endif
	uint flags = inMaterial.z;
	bool metal = (flags & MeshMetal) != 0u;
	float metalAge = 0.0;
	if (metal)
	{
		float forgeL1 = abs(inWorld.x - HeatCenter.x) + abs(inWorld.z - HeatCenter.y);
		if (inTint.z > 2.5)
		{
			vec3 rel = inWorld - inAccent.xyz;
			float c = cos(inAccent.w);
			float s = sin(inAccent.w);
			vec3 cavity = vec3(rel.x, rel.y * c - rel.z * s, rel.y * s + rel.z * c);
			bool spout = inTint.x > 0.5 && cavity.z < -MeltRadius * 0.85 && cavity.z > -MeltSpoutReach && abs(cavity.x) < MeltSpoutHalf && abs(cavity.y - MeltSpoutTop) < MeltSpoutDepth;
			if (!spout && (cavity.y < MeltFloor || cavity.y > MeltRim || length(cavity.xz) > CrucibleInnerRadius(cavity.y) - MeltMargin))
				discard;
			metalAge = MeltAge;
		}
		else if (inTint.z > 1.5)
		{
			if (forgeL1 < inTint.w)
				discard;
			metalAge = FloodFrontAge + (forgeL1 - inTint.w) * FloodAgeRate;
		}
		else
		{
			float front = inAccent.x + inUv.x * inTint.x;
			metalAge = inAccent.w - front;
			bool hiltOnly = inAccent.y < 0.0;
			if (metalAge < 0.0 || inUv.x > abs(inAccent.y) || inAccent.w >= inAccent.z + inUv.x * inTint.x || (hiltOnly && inWorld.z - HeatCenter.y < ForgeHiltLine) || (inTint.z > 0.5 && forgeL1 >= inTint.w))
				discard;
		}
	}
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
	if ((flags & MeshFlow) != 0u)
	{
		float across = (inWorld.x + inWorld.z) * 0.7071;
		float n = VeinNoise(vec3(across * 1.6, inWorld.y * 0.55 + time * 1.9, 3.0)) * 0.6 + VeinNoise(vec3(across * 4.2, inWorld.y * 1.3 + time * 3.1, 11.0)) * 0.4;
		float hot = smoothstep(0.45, 0.85, n);
		float crusted = smoothstep(0.58, 0.8, VeinNoise(vec3(across * 2.6, inWorld.y * 0.8 + time * 2.4, 7.0)));
		float body = inColor.a;
		albedo *= 1.0 - body;
		heat += (FlowDeep + FlowHot * hot) * (body * (1.0 - 0.75 * crusted) * (0.86 + 0.28 * max(-pc.Eye.w, 0.0)));
	}
	if ((flags & MeshSeam) != 0u)
	{
		float seam = inColor.a * inColor.a;
		albedo *= 1.0 - 0.6 * seam;
		heat += SeamHeat * (seam * (SeamBase + SeamPulse * max(-pc.Eye.w, 0.0)) * (1.0 + SeamWarn * clamp(inShading.x, 0.0, 1.0)));
	}
	if (layered)
		albedo *= 0.62 + 0.76 * inColor.b;
	bool rock = (flags & MeshRock) != 0u;
	float glaze = 0.0;
	if (rock)
	{
		float bare = 1.0 - smoothstep(0.3, 0.75, inColor.r);
		albedo *= mix(vec3(1.0), mix(vec3(0.9, 0.95, 1.06), vec3(1.08, 1.0, 0.9), inColor.a), bare);
		float streak = RainNoise(vec2((inWorld.x + inWorld.z) * 1.9, inWorld.y * 0.24)) * 0.65 + RainNoise(vec2((inWorld.x - inWorld.z) * 5.3, inWorld.y * 0.6)) * 0.35;
		glaze = inColor.g * smoothstep(0.3, 0.62, streak) * bare;
		layer.Normal = normalize(vec3(layer.Normal.xy * mix(1.5, 0.5, glaze), layer.Normal.z));
	}
	float translucency = 0.0;
#ifdef CRYSTAL
	if ((flags & MeshCrystal) != 0u)
	{
		float vein;
		vec3 crystal = CrystalVeins(inWorld, inColor.a, vein);
		vein *= smoothstep(0.55, 0.85, normalize(inNormal).y);
		albedo = mix(albedo, crystal * 0.55, vein);
		heat += crystal * (vein * CrystalGlow);
		translucency = CrystalTranslucency * vein;
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
	if ((material.Flags & MaterialTranslucent) != 0u)
	{
		translucency = material.Params.w * metallic;
		metallic = 0.0;
	}
	if ((flags & MeshHeated) != 0u)
	{
		float l1 = abs(inWorld.x - HeatCenter.x) + abs(inWorld.z - HeatCenter.y);
		float since = inAccent.z - (inAccent.x + max(inAccent.w - l1, 0.0) * inAccent.y);
		float hot = max(smoothstep(0.0, HeatSeconds, since), PreHeat * smoothstep(-PreHeatSeconds, 0.0, since));
		float seams = (1.0 - smoothstep(0.12, 0.62, height)) * 0.7 + (1.0 - occlusion) * 0.8;
		float mottle = 0.6 + 0.8 * abs(height - 0.5);
		heat += HeatGlow * (hot * (0.12 + seams) * mottle * max(inTint.a, 1.0));
		albedo = mix(albedo, albedo * vec3(1.25, 0.75, 0.6), hot);
	}
	vec2 metalSlope = vec2(0.0);
	if (metal)
	{
		float clock = inTint.z > 2.5 ? inTint.w : inAccent.w;
		float hotSeconds = inTint.y;
		vec2 p = inWorld.xz;
		float texel = length(fwidth(p));
		bool flood = inTint.z < 0.5;
		float seed = fract(inAccent.x * 0.137) * 40.0;
		float cool = smoothstep(hotSeconds - MetalCoolSpan, hotSeconds + SmoulderBlend, metalAge);
		float flow = inUv.x * 0.55 - clock * (flood ? 0.25 : mix(1.35, SmoulderFlow, cool));
		vec2 warp = vec2(VeinNoise(vec3(p * 0.8, flow * 0.6 + 3.0)), VeinNoise(vec3(p * 0.8 + 7.3, flow * 0.6 + 9.0))) - 0.5;
		float churn = MetalFbm(vec3(p * 1.7 + warp * 2.4, flow));
		float veins = 0.0;
		float scum = 0.0;
		if (!flood)
		{
			veins = 1.0 - abs(2.0 * VeinNoise(vec3(p * 2.9 + warp * 3.0, flow * 1.4 + 17.0)) - 1.0);
			scum = smoothstep(0.55, 0.78, VeinNoise(vec3(p * 1.1 - warp, flow * 0.5 + 31.0)));
		}
		vec3 liquid = mix(MetalDeep, MetalBright, smoothstep(0.3, 0.75, churn));
		liquid = mix(liquid, MetalWhite * 0.7, smoothstep(0.72, 0.95, churn) * 0.6);
		liquid *= 1.0 - 0.35 * smoothstep(0.88, 0.985, veins);
		liquid *= 1.0 - 0.6 * scum * smoothstep(0.5, 3.0, metalAge) * (1.0 - cool);
		vec3 ember = mix(SmoulderDeep, SmoulderBright, smoothstep(0.35, 0.8, churn)) * (0.85 + 0.15 * sin(clock * 1.7 + churn * 6.0));
		liquid = mix(liquid, ember, cool);
		liquid = mix(liquid, MetalWhite, 0.85 * exp(-metalAge * 4.0));
		float skin = 0.0;
		float edge = 0.0;
		float fissure = 0.0;
		if (cool > 0.0)
		{
			vec2 drift = vec2(clock * 0.03, -clock * 0.05);
			vec2 skinWarp = vec2(VeinNoise(vec3(p * 0.6 + drift, seed)), VeinNoise(vec3(p * 0.6 + 5.1 + drift, seed + 3.0))) - 0.5;
			float raft = (MetalFbm(vec3((p + drift) * 1.25 + skinWarp * 1.8, seed + 7.0)) - 0.5) * RaftContrast + 0.5;
			float threshold = mix(0.85, SkinThreshold, cool);
			float aa = max(texel * 2.5, 1e-3);
			skin = smoothstep(threshold - aa, threshold + 0.03 + aa, raft);
			edge = exp(-max(raft - threshold, 0.0) / SkinRim) * skin;
			vec2 cells = CrackCells((p + drift) * 2.2 + skinWarp * 1.2 + seed);
			fissure = (1.0 - smoothstep(0.02 - texel * 3.0, 0.02 + texel * 3.0, cells.y - cells.x)) * skin * smoothstep(0.5, 0.65, VeinNoise(vec3(p * 0.8, seed + 41.0)));
		}
		float tone = 0.7 + 0.6 * VeinNoise(vec3(p * 3.1, seed + 29.0));
		albedo = mix(MetalLiquidAlbedo, SkinAlbedo * tone, skin);
		roughness = mix(mix(0.08, 0.3, scum), mix(0.55, 0.85, VeinNoise(vec3(p * 5.0, seed))), skin);
		metallic = mix(1.0, 0.25, skin);
		occlusion = 1.0;
		heat += liquid * ((1.0 - skin) * (flood ? FloodGain : 1.0));
		heat += SmoulderBright * (edge * SkinRimGlow + fissure * FissureGlow) * (0.85 + 0.15 * sin(clock * 2.3 + seed));
		heat += SmoulderDeep * (skin * SkinUnderglow);
		float relief = churn * 0.06 * (1.0 - cool * 0.6) + veins * 0.015 + skin * 0.025 - fissure * 0.015;
		mat2 footprint = mat2(dFdx(p), dFdy(p));
		float area = determinant(footprint);
		if (abs(area) > 1e-8)
			metalSlope = inverse(transpose(footprint)) * vec2(dFdx(relief), dFdy(relief));
	}

	if ((flags & MeshObsidian) != 0u)
		layer.Normal = normalize(vec3(layer.Normal.xy * RockRelief, layer.Normal.z));
	vec3 geometric = normalize(inNormal);
	vec3 n = geometric;
	vec3 t = tangent.xyz - geometric * dot(geometric, tangent.xyz);
	if (layer.Normal.z < 0.9999 && dot(t, t) > 1e-10)
	{
		t = normalize(t);
		vec3 b = cross(geometric, t) * tangent.w;
		n = normalize(t * layer.Normal.x + b * layer.Normal.y + geometric * layer.Normal.z);
	}
	if (metal)
		n = normalize(geometric - vec3(metalSlope.x, 0.0, metalSlope.y));
	if ((flags & MeshObsidian) != 0u)
	{
		float obsidian = clamp(inUv.x, 0.0, 1.0);
		float vein = 1.0 - abs(2.0 * VeinNoise(inWorld * 0.9 + vec3(0.0, inWorld.x * 0.35, 0.0)) - 1.0);
		float marble = smoothstep(0.86, 0.97, vein) * 0.8 + smoothstep(0.55, 0.9, VeinNoise(inWorld * 3.1)) * 0.2;
		albedo = mix(albedo, ObsidianTint, obsidian);
		albedo += MarbleVein * (marble * obsidian);
		roughness = mix(roughness, mix(ObsidianRoughness, 0.26, marble), obsidian);
		n = normalize(mix(n, geometric, obsidian * 0.85));
		vec3 toEye = normalize(pc.Eye.xyz - inWorld);
		vec3 mirror = reflect(-toEye, n);
		float grazing = pow(1.0 - clamp(dot(n, toEye), 0.0, 1.0), 3.0);
		vec3 sheen = mirror.y >= 0.0 ? mix(GlassHorizon, GlassSky, smoothstep(0.0, 0.7, mirror.y)) : GlassLava * smoothstep(0.0, -0.5, mirror.y);
		float glint = smoothstep(0.78, 0.96, VeinNoise(geometric * 11.3 + inWorld * 0.07 + 5.1));
		heat += sheen * (obsidian * (GlassBase + GlassFresnel * grazing + GlassGlint * glint));
	}
	if ((flags & MeshPolished) != 0u)
	{
		vec3 toEye = normalize(pc.Eye.xyz - inWorld);
		vec3 mirror = reflect(-toEye, n);
		float grazing = pow(1.0 - clamp(dot(n, toEye), 0.0, 1.0), 2.0);
		vec3 lit = mix(PolishHorizon, PolishSky, smoothstep(0.15, 0.85, mirror.y));
		float glint = smoothstep(0.82, 0.97, VeinNoise(inWorld * 2.3 + mirror * 6.0));
		heat += albedo * (lit * (PolishBase + PolishFresnel * grazing) + PolishGlint * glint * (1.0 - roughness));
	}
	if (rock && glaze > 0.0)
	{
		albedo = mix(albedo, albedo * 0.5 + vec3(0.012, 0.026, 0.034), glaze);
		roughness = mix(roughness, 0.09, glaze);
		n = normalize(mix(n, geometric, glaze * 0.5));
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
	if (pc.Eye.w > 0.0 && !water && (flags & (MeshSkinned | MeshBright)) == 0u)
	{
		float rain = pc.Eye.w;
		float open = smoothstep(0.1, 0.7, geometric.y);
		float bright = smoothstep(0.3, 0.52, Luminance(albedo));
		float film = rain * mix(0.4, 1.0, open) * (1.0 - bright);
		float patches = smoothstep(0.62, 0.8, 0.7 * RainNoise(inWorld.xz * 0.32) + 0.3 * RainNoise(inWorld.xz * 1.1 + 7.3));
		float flat4 = open * open;
		float puddle = flat4 * flat4 * patches * rain * (1.0 - bright);
		albedo *= mix(1.0, 0.7, film * (1.0 - metallic));
		albedo *= mix(1.0, 0.82, puddle);
		roughness = mix(roughness, max(roughness * 0.55, 0.33), film);
		roughness = mix(roughness, 0.22, puddle);
		roughness = mix(roughness, roughness * 0.65, rain * bright);
		n = normalize(mix(n, geometric, max(puddle, film * 0.2)));
	}
	if (backFacing)
		n = -n;
	if (!metal && (flags & MeshHeated) == 0u && inAccent.a > 0.0)
	{
		float facing = clamp(dot(n, normalize(pc.Eye.xyz - inWorld)), 0.0, 1.0);
		heat += inAccent.rgb * (pow(1.0 - facing, RimPower) * inAccent.a);
	}
	if ((material.Flags & MaterialPbr) != 0u)
		roughness = FilterRoughness(n, roughness);
	else
		occlusion *= GroundOcclusion(inShading.y);

	vec3 tint = metal ? vec3(1.0) : inTint.rgb;
	bool twoTone = (flags & MeshTwoTone) != 0u;
	if (twoTone)
		tint = mix(inTint.rgb, inAccent.rgb, clamp(smoothstep(TwoToneLow, TwoToneHigh, Luminance(albedo)) + smoothstep(TwoToneFacing, 1.0, geometric.y) * TwoToneUp, 0.0, 1.0));
	albedo = mix(albedo * tint, vec3(1.0), (metal || (flags & MeshHeated) != 0u) ? 0.0 : clamp(inTint.a, 0.0, 1.0));
	if (twoTone)
		albedo = min(albedo, vec3(0.95));
	float glow = clamp(inShading.x, 0.0, 1.0);
	if ((flags & MeshLavaGlow) != 0u)
		heat += HotRock * glow;
	float fade = DepthFade(pc.Focus.x, inShading.z);
	vec3 emitted = albedo * glow + heat + (twoTone ? inAccent.rgb * (TwoToneVeins * Luminance(material.Extra.rgb)) : material.Extra.rgb) * layer.Surface.a;
	if ((flags & MeshBright) != 0u)
		emitted *= BrightGain;
	if (inFlash != 0u)
	{
		vec4 flash = unpackUnorm4x8(inFlash);
		emitted = mix(emitted, flash.rgb, flash.a);
		glow = mix(glow, 1.0, flash.a);
	}
#ifdef DISSOLVE
	albedo *= 1.0 - 0.9 * charred;
	emitted += inBurn.rgb * (burn * burn);
	translucency *= 1.0 - charred;
#endif
	outAlbedo = vec4(albedo * (1.0 - glow), occlusion);
	outSurface = PackSurface(n, roughness, metallic);
	outEmission = vec4(emitted * (1.0 - fade), PackEmissionTag(cut > 0.0, translucency * (1.0 - glow)));
}
