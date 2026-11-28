#version 450
#define MATERIAL_BINDING 7
#include "Material.glsl"
#include "Brdf.glsl"
#include "Environment.glsl"
#include "Glass.glsl"
#include "Dissolve.glsl"

layout(std430, set = 0, binding = 0) readonly buffer FrameBuffer { GlassFrame Frame; };
layout(set = 0, binding = 2) uniform sampler2D Pyramid;
layout(set = 0, binding = 3) uniform sampler2D BackFaces;
layout(set = 0, binding = 4) uniform sampler2D SceneDepth;
layout(set = 0, binding = 5) uniform sampler2D EnvironmentMap;

layout(push_constant) uniform PushConstants
{
	vec4 Absorb;
	vec4 Look;
} pc;

layout(location = 0) in vec3 inWorld;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec4 inTangent;
layout(location = 4) flat in uint inMaterial;
layout(location = 5) in vec3 inLocal;
layout(location = 6) flat in vec4 inDissolve;
layout(location = 7) flat in vec4 inBurn;
layout(location = 8) in vec4 inColor;
layout(location = 9) flat in vec4 inOrigin;
layout(location = 10) flat in uint inFlags;
layout(location = 11) flat in vec4 inTint;
layout(location = 12) flat in vec4 inAccent;
layout(location = 13) flat in float inGlow;

layout(location = 0) out vec4 outScene;
layout(location = 1) out vec4 outEmission;

#include "GlassSample.glsl"

const float SpellAmbient = 0.6;
const float WaterRoughness = 0.04;
const float CausticGain = 2.4;
const float FoamDepth = 0.05;
const float IceVeil = 0.3;
const float IceScatter = 1.5;
const float IceGlow = 0.3;
const float IceBlur = 4.0;
const float IceEdge = 0.6;
const float InteriorGlow = 0.5;
const vec2 InteriorDepths = vec2(0.12, 0.3);
const float TintDistance = 0.16;
const float GemIor = 1.55;
const float GemSpread = 0.06;
const float GemSurfaceGlow = 0.25;
const float GemEdge = 0.3;
const float GemVeil = 0.3;
const float GemStudio = 1.6;
const float GemCore = 0.06;
const float GemHue = 0.65;
const float GemDepth = 0.14;

float FloorHit(vec3 origin, vec3 direction, float floorY, out vec2 uv)
{
	uv = vec2(0.5);
	if (direction.y > -0.05)
		return 0.0;
	uv = ToScreen(origin + direction * max((floorY - origin.y) / direction.y, 0.0));
	vec2 inside = smoothstep(Frame.Region.xy, Frame.Region.xy + 0.03, uv) * (1.0 - smoothstep(Frame.Region.zw - 0.03, Frame.Region.zw, uv));
	return inside.x * inside.y * smoothstep(-0.05, -0.25, direction.y);
}

vec3 SpellGlow(vec3 position, vec3 n)
{
	vec3 total = vec3(0.0);
	for (uint i = 0u; i < uint(Frame.Translucent.y); i++)
	{
		vec4 place = Frame.Lights[i * 2u];
		vec3 toLight = place.xyz - position;
		float distance2 = dot(toLight, toLight);
		float window = max(1.0 - distance2 / (place.w * place.w), 0.0);
		vec3 l = toLight * inversesqrt(max(distance2, 1e-6));
		total += Frame.Lights[i * 2u + 1u].rgb * (window * window / (1.0 + 0.5 * distance2) * (SpellAmbient + max(-dot(n, l), 0.0)));
	}
	return total * InvPi;
}

vec3 Refracted(vec3 point, vec3 direction, float travel, vec2 screen, float frontZ, float level)
{
	vec2 offset = ToScreen(point + direction * clamp(travel, 0.0, 4.0)) - screen;
	float reach = length(offset * vec2(Frame.Screen.x / Frame.Screen.y, 1.0));
	offset *= min(1.0, Frame.Screen.w / max(reach, 1e-5));
	float leak = LinearDepth(textureLod(SceneDepth, screen + offset, 0.0).r) < frontZ - 0.05 ? 0.0 : 1.0;
	return SamplePyramid(screen + offset * leak, level);
}

vec3 Through(vec3 n, vec3 view, float eta, vec3 backNormal, bool exits, float travel, float beyond, vec2 screen, float frontZ, float level)
{
	vec3 entered = refract(-view, n, 1.0 / eta);
	vec3 direction = entered;
	if (exits)
	{
		vec3 escaped = refract(entered, backNormal, eta);
		direction = dot(escaped, escaped) > 1e-6 ? escaped : reflect(entered, backNormal);
	}
	return Refracted(inWorld + entered * travel, direction, beyond, screen, frontZ, level);
}

#if defined(WATER) || defined(RIVER)
vec2 WaterSlope(vec2 p, float time, vec2 origin)
{
	const vec4 waves[4] = vec4[](vec4(0.8, 0.6, 7.0, 1.3), vec4(-0.55, 0.83, 11.0, 1.9), vec4(0.28, -0.96, 17.0, 2.4), vec4(-0.9, -0.42, 23.0, 3.1));
	vec2 slope = vec2(0.0);
	for (int i = 0; i < 4; i++)
		slope += waves[i].xy * (cos(dot(p, waves[i].xy) * waves[i].z + time * waves[i].w) * 0.012 * waves[i].z / (1.0 + 0.08 * waves[i].z));
	vec2 radial = p - origin;
	float r = max(length(radial), 1e-3);
	slope += radial / r * (cos(r * 26.0 - time * 3.2) * 0.05 * exp(-r * 1.6));
	return slope;
}

float Caustic(vec2 p, float time)
{
	float a = 1.0 - abs(2.0 * DissolveNoise(vec3(p * 3.1, time * 0.35)) - 1.0);
	float b = 1.0 - abs(2.0 * DissolveNoise(vec3(p * 4.3 + 7.3, time * 0.43 + 3.1)) - 1.0);
	return pow(a, 9.0) + pow(b, 9.0) * 0.7;
}
#endif

#ifdef RIVER
layout(set = 0, binding = 8) uniform sampler2D SceneCopy;

const float RiverIor = 1.333;
const float RiverSpeed = 1.5;
const float RiverPhaseRate = 0.7;
const float RiverRoughness = 0.06;
const float RiverRefract = 0.6;
const float RiverRefractLimit = 0.05;
const float RiverReflect = 3.0;
const vec3 RiverScatterTint = vec3(0.24, 0.52, 0.46);
const float RiverAmbientLevel = 2.0;
const float RiverFoamEdge = 0.1;
const vec3 RiverFoamAlbedo = vec3(0.82, 0.86, 0.84);
const float RiverSheenRoughness = 0.13;
const float RiverGlintLimit = 3.0;
const float RiverCaustics = 0.55;
const float RiverCausticScale = 2.2;
const vec3 Luma = vec3(0.2126, 0.7152, 0.0722);

const float RiverPatternSpeed = 1.0;
const float RiverFallSpeed = 3.5;
const vec2 RiverWind = vec2(0.31, 0.18);
const float RiverLeafCell = 1.7;
const float RiverLeafDensity = 0.16;
const mat2 RiverTurn = mat2(1.62, 1.18, -1.18, 1.62);

vec2 RiverGradient(ivec2 c)
{
	uint h = uint(c.x) * 0x8da6b343u ^ uint(c.y) * 0xd8163841u;
	h ^= h >> 15;
	h *= 0x2c1b3c6du;
	h ^= h >> 12;
	h *= 0x297a2d39u;
	h ^= h >> 15;
	float angle = float(h) * (6.2831853 / 4294967296.0);
	return vec2(cos(angle), sin(angle));
}

float RiverHash(ivec2 c, uint salt)
{
	uint h = uint(c.x) * 0x9e3779b1u ^ uint(c.y) * 0x85ebca77u ^ salt * 0xc2b2ae3du;
	h ^= h >> 16;
	h *= 0x7feb352du;
	h ^= h >> 15;
	h *= 0x846ca68bu;
	h ^= h >> 16;
	return float(h & 0xFFFFFFu) / 16777215.0;
}

vec3 RiverNoise(vec2 p)
{
	ivec2 i = ivec2(floor(p));
	vec2 f = p - floor(p);
	vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	vec2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
	vec2 ga = RiverGradient(i);
	vec2 gb = RiverGradient(i + ivec2(1, 0));
	vec2 gc = RiverGradient(i + ivec2(0, 1));
	vec2 gd = RiverGradient(i + ivec2(1, 1));
	float va = dot(ga, f);
	float vb = dot(gb, f - vec2(1.0, 0.0));
	float vc = dot(gc, f - vec2(0.0, 1.0));
	float vd = dot(gd, f - vec2(1.0, 1.0));
	float k = va - vb - vc + vd;
	return vec3(va + u.x * (vb - va) + u.y * (vc - va) + u.x * u.y * k,
		ga + u.x * (gb - ga) + u.y * (gc - ga) + u.x * u.y * (ga - gb - gc + gd) + du * (u.yx * k + vec2(vb, vc) - va));
}

vec2 RiverFbm(vec2 p, int octaves)
{
	vec2 slope = vec2(0.0);
	mat2 chain = mat2(1.0);
	float amplitude = 1.0;
	for (int i = 0; i < octaves; i++)
	{
		slope += amplitude * (transpose(chain) * RiverNoise(p).yz);
		p = RiverTurn * p + vec2(17.3, -9.1);
		chain = RiverTurn * chain;
		amplitude *= 0.42;
	}
	return slope;
}

vec3 RiverLeaf(vec2 q, float pace, float time, out float mask)
{
	mask = 0.0;
	ivec2 c = ivec2(floor(q / RiverLeafCell));
	if (RiverHash(c, 1u) > RiverLeafDensity)
		return vec3(0.0);
	vec2 center = (vec2(c) + 0.25 + 0.5 * vec2(RiverHash(c, 2u), RiverHash(c, 3u))) * RiverLeafCell;
	vec2 d = q - center;
	d.y *= (0.45 + 0.55 * clamp(pace, 0.0, 1.0)) / RiverPatternSpeed;
	float angle = 6.2831853 * RiverHash(c, 4u) + time * 0.25 * (RiverHash(c, 5u) - 0.5);
	vec2 local = vec2(cos(angle) * d.x + sin(angle) * d.y, -sin(angle) * d.x + cos(angle) * d.y);
	float size = 0.06 + 0.06 * RiverHash(c, 6u);
	float x = local.x / size;
	float width = 0.48 * sqrt(max(1.0 - x * x, 0.0)) * (1.0 + 0.25 * cos(x * 7.0));
	float edge = abs(local.y) / size - width;
	float aa = fwidth(edge) + 1e-4;
	mask = (1.0 - smoothstep(-aa, aa, edge)) * step(abs(x), 1.0);
	float hue = RiverHash(c, 7u);
	vec3 color = hue < 0.35 ? vec3(0.72, 0.24, 0.05) : hue < 0.6 ? vec3(0.8, 0.5, 0.09) : hue < 0.85 ? vec3(0.52, 0.11, 0.04) : vec3(0.42, 0.33, 0.14);
	return color * (0.8 + 0.4 * RiverHash(c, 8u)) * (1.0 - 0.35 * (1.0 - smoothstep(0.0, 0.05, abs(local.y) / size)));
}

float RiverCaustic(vec2 p, float time)
{
	vec2 q = p * RiverCausticScale;
	vec2 i = q;
	float c = 1.0;
	for (int n = 0; n < 4; n++)
	{
		float t = time * 0.6 * (1.0 - 3.5 / float(n + 1));
		i = q + vec2(cos(t - i.x) + sin(t + i.y), sin(t - i.y) + cos(t + i.x));
		c += 1.0 / length(vec2(q.x / (sin(i.x + t) * 200.0), q.y / (cos(i.y + t) * 200.0)));
	}
	c = 1.17 - pow(c * 0.25, 1.4);
	return pow(abs(c), 8.0);
}

vec3 RiverSky(vec3 r)
{
	vec3 horizon = Frame.SkyRadiance.rgb * 0.55 + Frame.SunRadiance.rgb * 0.1;
	vec3 zenith = Frame.SkyRadiance.rgb * vec3(0.85, 0.97, 1.15);
	return mix(horizon, zenith, smoothstep(0.15, 0.95, r.y));
}

float RiverFoamNoise(vec2 p)
{
	float a = DissolveNoise(vec3(p * 3.1, 0.0));
	float b = DissolveNoise(vec3(p * 7.3 + 4.1, 1.7));
	float c = 1.0 - abs(2.0 * DissolveNoise(vec3(p * 1.6 + 9.3, 3.1)) - 1.0);
	return 0.45 * a + 0.3 * b + 0.25 * c;
}
#endif

void main()
{
	float burn = 0.0;
	if (inDissolve.x > 0.0)
	{
		float field = DissolveField(inLocal, inDissolve.z, inDissolve.w, inBurn.w);
		float threshold = DissolveThreshold(inDissolve.x, inDissolve.y);
		if (field <= threshold)
			discard;
		burn = DissolveEdge(field, threshold, inDissolve.y);
	}
	vec2 screen = gl_FragCoord.xy / Frame.Screen.xy;
	vec3 forward = Frame.Forward.xyz;
	vec3 ray = inWorld - Frame.Eye.xyz;
	float frontZ = dot(ray, forward);
	vec3 view = -normalize(ray);
	float sceneZ = LinearDepth(textureLod(SceneDepth, screen, 0.0).r);
	float buried = clamp((sceneZ - frontZ) / 0.06, 0.0, 1.0);
	if (buried <= 0.0)
		discard;
	float time = Frame.Eye.w;
	float ior = pc.Absorb.a;
	vec3 l = Frame.SunDirection.xyz;
	float cosRay = max(dot(-view, forward), 0.2);
	vec3 geometric = normalize(inNormal);

#if defined(RIVER)
	vec3 along = inTangent.xyz - geometric * dot(geometric, inTangent.xyz);
	along = dot(along, along) > 1e-8 ? normalize(along) : vec3(0.0, 0.0, 1.0);
	vec3 across = cross(geometric, along);
	float pace = inColor.r;
	float rapid = inColor.g;
	bool fall = inColor.a < 0.5;
	float speed = fall ? RiverFallSpeed : RiverPatternSpeed;
	vec2 q = vec2(-inUv.x, (inUv.y - time) * speed);
	vec2 world = inWorld.xz;
	float calm = smoothstep(0.25, 0.75, 0.5 + 0.9 * RiverNoise(world * 0.09 + vec2(time * 0.012, -3.7)).x);
	float strength = (0.55 + 0.45 * pace + 1.1 * rapid) * mix(0.4, 1.25, calm);
	vec2 slope = (RiverFbm(q * vec2(1.05, 0.62), 3) * vec2(1.05, 0.62) * 0.11 + RiverFbm(q * 2.7 + vec2(4.1, time * 0.45), 2) * 0.07) * strength;
	vec2 drift = vec2(0.0);
	float wake = 0.0;
	if (!fall)
	{
		drift = RiverFbm(world * 3.4 + RiverWind * time * 3.4 + vec2(-31.0, 12.0), 2) * 3.4 * 0.011 * mix(1.4, 0.6, calm);
		for (uint i = 0u; i < uint(Frame.WaterCounts.y); i++)
		{
			vec4 source = Frame.Ripples[i];
			vec2 d = world - source.xy;
			float r = length(d);
			if (r > 3.0)
				continue;
			drift += d / max(r, 1e-3) * (cos(r * 15.0 - time * 7.0 + source.w) * 0.2 * source.z * exp(-r * 1.6) * smoothstep(0.05, 0.3, r));
			wake = max(wake, source.z * (1.0 - smoothstep(0.2, 0.45, r)));
		}
	}
	float boil = fall ? 0.0 : inColor.b;
	if (boil > 0.0)
	{
		vec2 radial = world - pc.Look.zw;
		float r = max(length(radial), 1e-3);
		drift += radial / r * (cos(r * 11.0 - time * 4.0) * 0.22 * boil * exp(-r * 0.7));
	}
	vec3 n = normalize(geometric - across * slope.x - along * slope.y - vec3(drift.x, 0.0, drift.y));
	float cosV = clamp(dot(n, view), 1e-3, 1.0);
	float ambient = dot(SamplePyramid(screen, 2.5), Luma);
	float shade = clamp((ambient - 0.04) / 0.22, 0.12, 1.0);
	vec3 litWhite = (Frame.SunRadiance.rgb * (max(l.y, 0.0) * shade) + Frame.SkyRadiance.rgb) * InvPi;
	vec3 foamLit = RiverFoamAlbedo * litWhite;
	vec3 glints = min(EvaluateBrdf(vec3(0.0), 0.0, RiverRoughness, n, view, l) * Frame.SunRadiance.rgb * max(dot(n, l), 0.0)
		+ EvaluateBrdf(vec3(0.0), 0.0, RiverSheenRoughness, n, view, Frame.Sheen.xyz) * Frame.SheenRadiance.rgb * max(dot(n, Frame.Sheen.xyz), 0.0), vec3(RiverGlintLimit)) * (shade * mix(0.5, 1.0, calm));
	vec3 radiance;
	float coverage = buried;
	if (fall)
	{
		float drop = inColor.b;
		float streak = 0.6 * (0.5 + 0.7 * RiverNoise(vec2(q.x * 6.0, q.y * 0.6)).x) + 0.4 * (0.5 + 0.7 * RiverNoise(vec2(q.x * 17.0, q.y * 1.3 + 7.0)).x);
		float white = clamp(smoothstep(0.5, 0.78, streak) * (0.7 + 0.3 * rapid) + 0.3 * smoothstep(0.1, 0.5, drop) * smoothstep(0.35, 0.65, streak), 0.0, 1.0);
		vec3 body = RiverScatterTint * ambient * RiverAmbientLevel * 0.6 + RiverSky(reflect(-view, n)) * FresnelDielectric(cosV, RiverIor) * RiverReflect;
		radiance = mix(body, foamLit * 1.15, white) + glints;
		float breakup = 0.5 + 0.7 * RiverNoise(vec2(q.x * 3.5, q.y * 0.8 + 3.0)).x;
		coverage *= (1.0 - smoothstep(0.45, 1.0, drop + 0.5 * breakup * drop)) * (0.6 + 0.4 * white) * smoothstep(0.0, 0.25, inColor.g) * smoothstep(0.0, 0.06, inUv.y);
	}
	else
	{
		float path = max(sceneZ - frontZ, 0.0) / cosRay;
		float vertical = path * clamp(view.y, 0.05, 1.0);
		vec3 entered = refract(-view, n, 1.0 / RiverIor);
		vec3 bed = inWorld + entered * (vertical / max(-entered.y, 0.3));
		vec2 offset = (ToScreen(bed) - ToScreen(inWorld - view * path)) * RiverRefract;
		float reach = length(offset * vec2(Frame.Screen.x / Frame.Screen.y, 1.0));
		offset *= min(1.0, RiverRefractLimit / max(reach, 1e-5));
		vec2 bent = screen + offset;
		if (LinearDepth(textureLod(SceneDepth, bent, 0.0).r) < frontZ + 0.02)
			bent = screen;
		vec3 seen = textureLod(SceneCopy, bent, 0.0).rgb;
		float sunlit = clamp(dot(seen, Luma) * 4.0, 0.0, 1.0);
		seen *= 1.0 + min(RiverCaustic(bed.xz, time), 2.5) * RiverCaustics * sunlit * smoothstep(0.02, 0.15, vertical) * exp(-vertical * 1.6);
		float body = 0.0;
		float shadow = 0.0;
		vec2 castOffset = l.xz / max(l.y, 0.3) * 0.2;
		for (uint i = 0u; i < uint(Frame.WaterCounts.x); i++)
		{
			vec4 fish = Frame.Fish[i];
			vec2 d = bed.xz - fish.xy;
			if (dot(d, d) > 1.2)
				continue;
			vec2 heading = vec2(cos(fish.z), sin(fish.z));
			for (int pass = 0; pass < 2; pass++)
			{
				vec2 e = pass == 0 ? d : d - castOffset;
				vec2 local = vec2(dot(e, heading), dot(e, vec2(-heading.y, heading.x)));
				float len = 0.18 * fish.w;
				float tail = smoothstep(0.1 * len, -len, local.x);
				local.y += sin(local.x * 13.0 / fish.w - time * 8.0 + float(i) * 1.7) * 0.03 * fish.w * tail;
				float width = 0.055 * fish.w * (1.0 - 0.65 * tail);
				float shape = length(vec2(local.x / len, local.y / width));
				float fin = step(-1.35 * len, local.x) * step(local.x, -0.8 * len) * step(abs(local.y), (-0.8 * len - local.x) * 0.9);
				float soft = pass == 0 ? 0.15 : 0.45;
				float mask = max(1.0 - smoothstep(1.0 - soft, 1.0, shape), fin * (pass == 0 ? 0.8 : 0.5));
				if (pass == 0)
					body = max(body, mask);
				else
					shadow = max(shadow, mask);
			}
		}
		float deepEnough = smoothstep(0.12, 0.25, vertical);
		seen *= 1.0 - 0.4 * shadow * deepEnough;
		vec3 transmit = exp(-pc.Absorb.rgb * path);
		float murk = 1.0 - exp(-pc.Look.x * path);
		vec3 under = seen * transmit * (1.0 - murk) + RiverScatterTint * (ambient * RiverAmbientLevel) * murk;
		under = mix(under, under * vec3(0.3, 0.33, 0.28), body * deepEnough * 0.8);
		float fresnel = FresnelDielectric(cosV, RiverIor);
		radiance = under * (1.0 - fresnel) + RiverSky(reflect(-view, n)) * (fresnel * RiverReflect) + glints;
		float edge = 1.0 - smoothstep(0.0, RiverFoamEdge, vertical);
		float amount = clamp(0.5 * edge + 0.85 * rapid + 0.45 * boil + 0.7 * wake, 0.0, 1.0);
		float pattern = RiverFoamNoise(q * vec2(1.0, 0.75));
		float foam = smoothstep(1.0 - amount, 1.12 - amount, pattern) * amount;
		radiance = mix(radiance, foamLit, foam * 0.85);
		coverage *= smoothstep(0.0, 0.012, vertical) * (1.0 - smoothstep(2.5, 4.0, vertical));
		float leaf = 0.0;
		vec3 leafColor = RiverLeaf(q, pace, time, leaf);
		leaf *= smoothstep(0.02, 0.06, vertical) * (1.0 - smoothstep(2.5, 4.0, vertical));
		radiance = mix(radiance, leafColor * litWhite * 1.1, leaf);
		coverage = max(coverage, leaf * buried);
		glints *= 1.0 - leaf;
	}
	vec3 glow = glints * 0.25;
#elif defined(WATER)
	vec2 slope = WaterSlope(inWorld.xz, time, inOrigin.xz);
	vec3 n = normalize(geometric + vec3(-slope.x, 0.0, -slope.y));
	float roughness = WaterRoughness;
	float depth = max(sceneZ - frontZ, 0.0) / cosRay;
	float cosV = clamp(dot(n, view), 1e-3, 1.0);
	float fresnel = FresnelDielectric(cosV, ior);
	vec3 entered = refract(-view, n, 1.0 / ior);
	vec3 floorPoint = inWorld + entered * depth;
	vec3 seen = Refracted(inWorld, entered, depth, screen, frontZ, 0.0);
	float lit = clamp(dot(seen, vec3(0.3333)) * 4.0, 0.0, 1.0);
	seen *= 1.0 + Caustic(floorPoint.xz, time) * CausticGain * lit * exp(-depth * 1.5);
	vec3 absorption = exp(-pc.Absorb.rgb * depth);
	vec3 deep = Frame.SkyRadiance.rgb * vec3(0.05, 0.15, 0.16) * pc.Look.y;
	vec3 reflected = Sky(reflect(-view, n), roughness);
	vec3 radiance = (seen * absorption + deep * (1.0 - absorption)) * (1.0 - fresnel) + reflected * fresnel;
	vec3 specular = EvaluateBrdf(vec3(0.0), 0.0, roughness, n, view, l) * Frame.SunRadiance.rgb * max(dot(n, l), 0.0);
	float edge = max(sceneZ - frontZ, 0.0);
	float foam = (1.0 - smoothstep(0.0, FoamDepth, edge)) * smoothstep(0.35, 0.75, DissolveNoise(vec3(inWorld.xz * 14.0, time * 0.8)));
	vec3 foamLit = vec3(0.85) * (Frame.SunRadiance.rgb * max(geometric.y * dot(geometric, l), 0.0) * InvPi + Frame.SkyRadiance.rgb);
	radiance = mix(radiance + specular, foamLit, foam * 0.8);
	vec3 glow = specular * 0.3;
#else
	Material material = materials[inMaterial];
	vec2 st = inUv * material.Params.z;
	vec3 albedo = texture(Textures[nonuniformEXT(material.Albedo)], st).rgb * material.Color.rgb;
	if ((inFlags & MeshVertexTint) != 0u)
		albedo *= inColor.rgb;
	albedo = mix(albedo * inTint.rgb, vec3(1.0), clamp(inTint.a, 0.0, 1.0));
	vec4 surface = texture(Textures[nonuniformEXT(material.Surface)], st);
	vec3 n = geometric;
	vec3 t = inTangent.xyz - geometric * dot(geometric, inTangent.xyz);
	if (material.Normal != 0u && dot(t, t) > 1e-10)
	{
		vec2 xy = texture(Textures[nonuniformEXT(material.Normal)], st).rg * 2.0 - 1.0;
		t = normalize(t);
		n = normalize(t * xy.x + cross(geometric, t) * (inTangent.w * xy.y) + geometric * sqrt(max(1.0 - dot(xy, xy), 0.0)));
	}
	float roughness = surface.g * material.Params.x;
	float thin = (material.Flags & MaterialTranslucent) != 0u ? surface.b : 1.0;
	float clarity = (1.0 - smoothstep(0.16, 0.45, roughness)) * pc.Look.x * (1.0 - clamp(inTint.a, 0.0, 1.0));
	float cosI = dot(n, l);
	float cosV = clamp(dot(n, view), 1e-3, 1.0);
	vec3 sun = Frame.SunRadiance.rgb * max(cosI, 0.0);
	vec3 sky = HemisphereRadiance(Frame.SkyRadiance.rgb, Frame.GroundRadiance.rgb, n.y, 1.0);
	vec3 frost = EvaluateBrdf(albedo, 0.0, roughness, n, view, l) * sun + albedo * sky * surface.r;
	vec3 inside = SpellGlow(inWorld, n) * (sqrt(albedo) * thin * IceGlow * (1.0 - pc.Look.w));

	vec4 back = texelFetch(BackFaces, ivec2(gl_FragCoord.xy), 0);
	bool exits = back.w > frontZ && back.w < sceneZ;
	float travel = max((back.w > frontZ ? min(back.w, sceneZ) : min(frontZ + 0.15, sceneZ)) - frontZ, 0.0) / cosRay;
	vec3 entered = refract(-view, n, 1.0 / ior);
	vec3 direction = entered;
	float beyond = 0.0;
	if (exits)
	{
		vec3 escaped = refract(entered, -normalize(back.xyz), ior);
		direction = dot(escaped, escaped) > 1e-6 ? escaped : reflect(entered, -normalize(back.xyz));
		beyond = (sceneZ - back.w) / cosRay;
	}
	vec3 exitPoint = inWorld + entered * travel;
	vec3 seen = Refracted(exitPoint, direction, beyond, screen, frontZ, roughness * IceBlur);
	if (ior > GemIor && pc.Look.z > 0.5)
	{
		vec3 backNormal = -normalize(back.xyz);
		seen.r = Through(n, view, ior - GemSpread, backNormal, exits, travel, beyond, screen, frontZ, roughness * IceBlur).r;
		seen.b = Through(n, view, ior + GemSpread, backNormal, exits, travel, beyond, screen, frontZ, roughness * IceBlur).b;
	}
	vec3 absorption = exp(-pc.Absorb.rgb * travel) * pow(max(inTint.rgb, vec3(0.02)), vec3(travel / TintDistance));
	vec3 veil = frost * (IceVeil + pc.Look.y * (1.0 - exp(-travel * IceScatter)));
	float fresnel = FresnelDielectric(cosV, ior);
	vec3 bounce = reflect(-view, n);
	vec3 reflected = Sky(bounce, roughness);
	vec2 floorUv;
	float floorWeight = FloorHit(inWorld, bounce, inOrigin.w, floorUv);
	if (floorWeight > 0.0)
		reflected = mix(reflected, SamplePyramid(floorUv, 1.5 + roughness * 6.0), floorWeight);
	vec3 specular = EvaluateBrdf(vec3(0.0), 0.0, roughness, n, view, l) * sun;
	vec3 dpx = dFdx(inWorld);
	vec3 dpy = dFdy(inWorld);
	vec2 dux = dFdx(st);
	vec2 duy = dFdy(st);
	vec3 across = cross(dpy, geometric);
	vec3 down = cross(geometric, dpx);
	float area = dot(dpx, across);
	vec3 interior = vec3(0.0);
	if (abs(area) > 1e-12)
		for (int k = 0; k < 2; k++)
		{
			vec3 drift = entered * (InteriorDepths[k] / max(dot(-entered, geometric), 0.25));
			vec2 shift = (dux * dot(drift, across) + duy * dot(drift, down)) / area;
			float plane = texture(Textures[nonuniformEXT(material.Surface)], st + shift).a;
			interior += material.Extra.rgb * inTint.rgb * (plane * InteriorGlow * exp(-pc.Absorb.rgb * InteriorDepths[k]));
		}
	float facet = smoothstep(0.15, 0.5, length(fwidth(geometric)));
	bool gem = ior > GemIor;
	vec3 hue = gem ? mix(vec3(1.0), inTint.rgb, GemHue) : vec3(1.0);
	vec3 edge = (Frame.SkyRadiance.rgb + Frame.SunRadiance.rgb * 0.15) * (facet * IceEdge * (ior > GemIor ? GemEdge : 1.0));
	if (gem)
		veil *= GemVeil;
	vec3 clear = (seen * absorption + veil) * (1.0 - fresnel) + reflected * (fresnel * hue) + specular;
	if (gem)
	{
		float cut = fract(sin(dot(floor(geometric * 6.0 + 0.5), vec3(12.9898, 78.233, 37.719))) * 43758.5453);
		vec3 r = reflect(-view, n);
		float studio = pow(max(dot(r, normalize(vec3(0.35, 0.85, 0.4))), 0.0), 20.0) + 0.7 * pow(max(dot(r, normalize(vec3(-0.6, 0.55, -0.35))), 0.0), 28.0);
		float saturation = max(max(inTint.r, inTint.g), inTint.b) - min(min(inTint.r, inTint.g), inTint.b);
		clear = clear * (0.6 + 0.8 * cut) + (vec3(0.55) * hue + Frame.SunRadiance.rgb * 0.06) * (studio * GemStudio) + albedo * ((GemCore + GemDepth * saturation) * (0.4 + 0.6 * cut));
		edge *= hue;
	}
	clear += interior + edge;
	vec3 radiance = mix(frost + inside, clear + inside, clarity);
	float lining = ior > GemIor ? GemSurfaceGlow : 1.0;
	vec3 glow = material.Extra.rgb * inTint.rgb * (surface.a * lining) + albedo * clamp(inGlow, 0.0, 1.0) + inAccent.rgb * (pow(1.0 - cosV, 3.5) * inAccent.a);
	radiance += glow;
#endif
#ifdef RIVER
	buried = coverage;
#endif
	vec3 burnLight = inBurn.rgb * (burn * burn);
	radiance += burnLight;
	glow += burnLight;
	outScene = vec4(radiance * buried, buried);
	outEmission = vec4(glow * buried, 0.0);
}
