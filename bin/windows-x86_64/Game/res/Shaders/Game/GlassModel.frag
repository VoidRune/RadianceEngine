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

#ifdef WATER
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

#ifdef WATER
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
	vec3 inside = SpellGlow(inWorld, n) * (sqrt(albedo) * thin * IceGlow);

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
	vec3 burnLight = inBurn.rgb * (burn * burn);
	radiance += burnLight;
	glow += burnLight;
	outScene = vec4(radiance * buried, buried);
	outEmission = vec4(glow * buried, 0.0);
}
