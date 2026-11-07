#version 450
#include "GBuffer.glsl"
#include "Material.glsl"
#include "Lake.glsl"

layout(std430, set = 0, binding = 4) readonly buffer IceField
{
	vec4 Field[];
};

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 Focus;
	vec4 Eye;
	vec4 FieldSize;
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

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outSurface;
layout(location = 2) out vec4 outEmission;

const uint NoiseTexture = 1u;
const vec3 BareIce = vec3(0.006, 0.016, 0.022);
const vec3 Hoar = vec3(0.54, 0.66, 0.76);
const vec3 Thin = vec3(0.1, 0.78, 0.74);
const vec3 WallDeep = vec3(0.012, 0.21, 0.26);
const vec3 WallBright = vec3(0.26, 0.86, 0.85);
const vec3 SnowLit = vec3(0.93, 0.95, 0.98);
const vec3 SnowHollow = vec3(0.7, 0.81, 0.97);
const vec2 WindAxis = vec2(0.88, 0.47);
const vec3 ToMoon = vec3(0.72, 0.66, 0.2);

vec4 SampleField(vec2 p)
{
	vec2 size = pc.FieldSize.xy;
	vec2 q = p - 0.5;
	vec2 i = floor(q);
	vec2 f = q - i;
	ivec2 a = clamp(ivec2(i), ivec2(0), ivec2(size) - 1);
	ivec2 b = clamp(ivec2(i) + 1, ivec2(0), ivec2(size) - 1);
	int w = int(size.x);
	vec4 s00 = Field[a.y * w + a.x];
	vec4 s10 = Field[a.y * w + b.x];
	vec4 s01 = Field[b.y * w + a.x];
	vec4 s11 = Field[b.y * w + b.x];
	return mix(mix(s00, s10, f.x), mix(s01, s11, f.x), f.y);
}

float Noise(vec2 p)
{
	return texture(Textures[NoiseTexture], p).r;
}

float SnowHeight(vec2 p)
{
	float mounds = Noise(vec2(p.x * 0.79 - p.y * 0.61, p.x * 0.61 + p.y * 0.79) / 118.0 + 0.21);
	float lumps = Noise(vec2(p.x * 0.34 + p.y * 0.94, p.y * 0.34 - p.x * 0.94) / 41.0 + 0.63);
	float pits = Noise(vec2(p.x * 0.92 + p.y * 0.39, p.y * 0.92 - p.x * 0.39) / 15.3 + 0.83);
	float crustBits = Noise(vec2(p.x * 0.5 - p.y * 0.87, p.x * 0.87 + p.y * 0.5) / 5.9 + 0.41);
	return mounds * 0.52 + lumps * 0.28 + pits * 0.15 + crustBits * 0.05;
}

float Glitter(vec2 p, vec3 n)
{
	vec2 cell = floor(p * 26.0);
	vec3 h = fract(vec3(cell.xyx) * vec3(0.1031, 0.103, 0.0973));
	h += dot(h, h.yzx + 33.33);
	h = fract((h.xxy + h.yzz) * h.zyx);
	vec2 inside = fract(p * 26.0) - 0.5;
	if (h.z < 0.95 || dot(inside, inside) > 0.1)
		return 0.0;
	vec3 facet = normalize(n + vec3(h.x - 0.5, 0.0, h.y - 0.5) * 1.5);
	vec3 halfway = normalize(normalize(pc.Eye.xyz - inWorld) + normalize(ToMoon));
	float aligned = max(dot(facet, halfway), 0.0);
	float a2 = aligned * aligned;
	a2 *= a2;
	a2 *= a2;
	a2 *= a2;
	return a2 * a2;
}

void main()
{
	vec3 geometric = normalize(inNormal);
	vec2 p = inWorld.xz;
	Material snow = materials[inMaterial.x];
	vec4 field = SampleField(p);
	if ((inMaterial.z & (MeshWet | MeshGround)) != 0u)
		field = vec4(0.0);
	float glowFlash = 0.0;
	vec3 flashColor = vec3(0.0);
	if (inFlash != 0u)
	{
		vec4 flash = unpackUnorm4x8(inFlash);
		glowFlash = flash.a;
		flashColor = flash.rgb;
	}
	float broad = Noise(p / 170.0);
	float mid = Noise(p / 52.0 + 0.3);
	bool land = (inMaterial.z & MeshGround) != 0u;
	if (geometric.y > 0.55 || land)
	{
		vec2 wander = vec2(broad, mid) - 0.5;
		vec2 wind = vec2(dot(p, WindAxis), dot(p, vec2(-WindAxis.y, WindAxis.x))) + wander * 7.0;
		float jag = Noise(p / 11.0 + 0.47);
		float crumb = Noise(p / 3.3 + 0.19);
		float reach = max(field.y, inColor.a);
		float edge = reach + (mid - 0.5) * 0.62 + (jag - 0.5) * 0.46 + (crumb - 0.5) * 0.26;
		float crust = smoothstep(0.43, 0.5, edge);
		float shoreline = 1.0;
		if (land)
		{
			shoreline = inWorld.y - pc.FieldSize.z - 0.04 - (jag - 0.5) * 0.34 - (crumb - 0.5) * 0.16;
			crust *= smoothstep(0.14, 0.5 + 0.4 * mid, shoreline);
		}
		float powder = smoothstep(0.26, 0.45, edge) * smoothstep(0.42, 0.62, crumb) * (1.0 - crust);
		float patches = land ? 0.0 : smoothstep(0.47, 0.67, broad + 0.08 * (mid - 0.5));
		float dust = 0.0;
		float rime = 0.0;
		if (patches > 0.0 && crust < 1.0)
		{
			float streaks = Noise(vec2(wind.x / 240.0, wind.y / 26.0) + 0.13) * 0.7 + Noise(vec2(wind.x / 110.0, wind.y / 9.0) + 0.57) * 0.3;
			dust = smoothstep(0.56, 0.84, streaks) * patches * (0.16 + 0.42 * crumb);
			rime = smoothstep(0.6, 0.76, Noise(p / 24.0 + 0.71) * 0.6 + broad * 0.4) * patches;
		}
		float frost = clamp(max(max(rime * 0.7, dust), powder * 0.85) * (1.0 - crust), 0.0, 1.0);
		float thin = field.x * (1.0 - crust);
		float warn = field.z * (1.0 - crust * 0.6) * smoothstep(0.34, 0.66, Noise(p / 29.0 + 0.09) * 0.65 + crumb * 0.35);
		float polish = mix(0.1, 0.16, smoothstep(0.35, 0.65, mid));
		float dx = 0.0;
		float dy = 0.0;
		if (crust < 1.0)
		{
			dx = Noise(p / 170.0 + vec2(0.008, 0.0)) - broad;
			dy = Noise(p / 170.0 + vec2(0.0, 0.008)) - broad;
		}
		vec2 slant = geometric.xz / max(geometric.y, 0.35);
		float haze = smoothstep(0.36, 0.7, mid * 0.65 + jag * 0.35);
		float breath = smoothstep(0.42, 0.72, broad * 0.5 + jag * 0.5);
		vec3 n = normalize(vec3(dx * 0.2 + slant.x, 1.0, dy * 0.2 + slant.y));
		vec3 base = mix(BareIce, Hoar, max(frost * 0.78, haze * breath * 0.05));
		float cloudy = haze * (0.35 + 0.65 * breath);
		float roughness = mix(mix(polish, 0.27, cloudy), 0.62, frost);
		float occlusion = 0.9;
		vec3 sparkle = vec3(0.0);
		if (crust > 0.0)
		{
			const float Step = 0.09;
			float centre = SnowHeight(p);
			vec2 stepX = vec2(Step, 0.0);
			vec2 stepY = vec2(0.0, Step);
			float gx = SnowHeight(p + stepX) - centre;
			float gy = SnowHeight(p + stepY) - centre;
			vec2 st = vec2(p.x * 0.64 - p.y * 0.77, p.x * 0.77 + p.y * 0.64) * snow.Params.z * 0.43 + 0.17;
			vec2 fine = snow.Normal != 0u ? texture(Textures[nonuniformEXT(snow.Normal)], st).rg * 2.0 - 1.0 : vec2(0.0);
			vec3 sn = normalize(vec3(-gx / Step * 0.95 + fine.x * 0.5 + slant.x, 1.0, -gy / Step * 0.95 + fine.y * 0.5 + slant.y));
			float hollow = smoothstep(0.56, 0.38, centre);
			float grain = texture(Textures[nonuniformEXT(snow.Albedo)], st).r;
			vec3 lit = mix(SnowLit, SnowHollow, hollow * 0.6) * (0.92 + 0.1 * grain);
			base = mix(base, lit, crust);
			n = normalize(mix(n, sn, crust));
			roughness = mix(roughness, 0.74 + 0.16 * crumb, crust);
			occlusion = mix(occlusion, 0.84 + 0.16 * (1.0 - hollow), crust);
			sparkle = vec3(0.85, 0.94, 1.0) * (Glitter(p, sn) * crust * 1.9);
		}
		if (land)
		{
			Material stone = materials[inMaterial.y];
			vec2 at = p * stone.Params.z;
			float wetBand = 1.0 - smoothstep(0.0, 0.32, shoreline);
			float bare = 1.0 - smoothstep(0.12, 0.5 + 0.4 * mid, shoreline);
			float rock = max(smoothstep(0.38, 0.6, inColor.g + (crumb - 0.5) * 0.16 + (jag - 0.5) * 0.3), bare);
			vec3 rockTex = vec3(0.0);
			vec3 rockN = geometric;
			vec4 arm = vec4(0.8, 0.82, 0.0, 1.0);
			if (rock > 0.002)
			{
				vec3 axes = pow(abs(geometric), vec3(4.0));
				axes /= axes.x + axes.y + axes.z;
				if (axes.y > 0.9)
					rockTex = texture(Textures[nonuniformEXT(stone.Albedo)], at).rgb;
				else
					rockTex = texture(Textures[nonuniformEXT(stone.Albedo)], at).rgb * axes.y + texture(Textures[nonuniformEXT(stone.Albedo)], inWorld.zy * stone.Params.z).rgb * axes.x + texture(Textures[nonuniformEXT(stone.Albedo)], inWorld.xy * stone.Params.z).rgb * axes.z;
				bool top = axes.y >= max(axes.x, axes.z);
				bool facingX = axes.x > axes.z;
				vec2 st = top ? at : (facingX ? inWorld.zy : inWorld.xy) * stone.Params.z;
				vec3 t = top ? vec3(1.0, 0.0, 0.0) : (facingX ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0));
				vec3 b = top ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
				arm = texture(Textures[nonuniformEXT(stone.Surface)], st);
				if (stone.Normal != 0u)
				{
					vec2 xy = texture(Textures[nonuniformEXT(stone.Normal)], st).rg * 2.0 - 1.0;
					rockN = normalize(geometric + (t - geometric * dot(geometric, t)) * xy.x * 1.3 + (b - geometric * dot(geometric, b)) * xy.y * 1.3);
				}
			}
			vec3 rockTone = rockTex * stone.Color.rgb * vec3(0.62, 0.68, 0.76);
			rockTone *= mix(1.0, 0.4, wetBand);
			rockTone = mix(rockTone, Hoar * 0.55, bare * (1.0 - wetBand) * smoothstep(0.45, 0.7, crumb) * 0.6);
			base = mix(base, rockTone, rock);
			n = normalize(mix(n, mix(rockN, geometric, wetBand * 0.5), rock));
			roughness = mix(roughness, mix(clamp(arm.g * stone.Params.x, 0.45, 0.95), 0.4, wetBand), rock);
			occlusion = mix(occlusion, 0.55 + 0.4 * arm.r, rock);
			sparkle *= 1.0 - rock;
			base *= inColor.r;
			sparkle *= inColor.r;
		}
		base = mix(base, Thin * 0.42, thin * 0.42);
		roughness = mix(roughness, 0.1, thin * 0.8);
		vec3 glow = Thin * (thin * thin * 0.08 + warn * warn * 0.24) + sparkle;
		outAlbedo = vec4(base, occlusion);
		outSurface = PackSurface(n, roughness, 0.0);
		outEmission = vec4(mix(glow, flashColor, glowFlash), land ? 0.52 : TagIce);
		return;
	}
	if ((inMaterial.z & MeshWet) != 0u)
	{
		outAlbedo = vec4(vec3(0.3, 0.44, 0.48) * (0.8 + 0.4 * mid), 1.0);
		outSurface = PackSurface(geometric, 0.18, 0.0);
		outEmission = vec4(vec3(0.0), TagWall);
		return;
	}
	float h = clamp((inWorld.y - pc.FieldSize.z) / max(pc.FieldSize.w - pc.FieldSize.z, 0.01), 0.0, 1.0);
	float strata = Noise(vec2((p.x + p.y) * 0.031, inWorld.y * 0.11) + 0.13);
	float sheen = Noise(vec2((p.x - p.y) * 0.019, inWorld.y * 0.04) + 0.41);
	float veil = Noise(vec2(p.x * 0.8 - p.y * 0.6, inWorld.y * 1.3) / 17.0 + 0.57);
	vec3 base = mix(WallDeep * 0.5, WallBright * 0.62, smoothstep(0.2, 1.1, h * 0.85 + strata * 0.5 - 0.15));
	base = mix(base, vec3(0.62, 0.86, 0.9), smoothstep(0.62, 0.82, veil) * 0.45);
	base *= inColor.rgb;
	float cap = smoothstep(0.72, 0.95, h) * smoothstep(0.35, 0.7, field.y);
	base = mix(base, SnowLit, cap);
	outAlbedo = vec4(base, 1.0);
	outSurface = PackSurface(geometric, mix(0.14, 0.32, strata), 0.0);
	vec3 glow = mix(WallDeep * 0.4, WallBright, h * h * h) * (0.03 + 0.15 * sheen) * inColor.r * (1.0 - cap);
	outEmission = vec4(mix(glow, flashColor, glowFlash), TagWall);
}
