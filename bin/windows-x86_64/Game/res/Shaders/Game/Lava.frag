#version 450
#include "GBuffer.glsl"

layout(set = 0, binding = 1) uniform sampler2D Crust;

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 Params;
} pc;

layout(location = 0) in vec3 inWorld;
layout(location = 1) flat in float inTime;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outSurface;
layout(location = 2) out vec4 outEmission;

const float Period = 16.0;
const vec2 Drift = vec2(0.025, -0.055);
const vec3 CrustColor = vec3(0.028, 0.024, 0.026);
const vec3 BasaltRed = vec3(0.07, 0.03, 0.022);
const vec3 SkinColor = vec3(0.16, 0.05, 0.02);
const vec3 Deep = vec3(0.52, 0.1, 0.014);
const vec3 Gold = vec3(0.92, 0.34, 0.05);
const vec3 WhiteHot = vec3(1.8, 1.15, 0.42);
const vec3 SkinGlow = vec3(0.24, 0.042, 0.007);
const vec3 Fissure = vec3(1.5, 0.42, 0.07);
const float CrustTranslucency = 0.75;
const float FuryMelt = 0.08;
const float MoltenLine = 0.62;
const float SkinLine = 0.47;
const float BubbleScale = 3.0;
const float BubbleRate = 0.45;
const float BubbleSize = 0.3;
const float CrustRelief = 0.3;
const float PlateLow = 1.3;
const float PlateHigh = 2.4;
const float GapNarrow = 0.07;
const float GapWide = 0.22;
const float PlateBevel = 0.16;
const float WallGlow = 0.1;
const float PoolShare = 0.05;
const float GapGain = 0.6;
const float GapCooled = 0.1;
const float SecondaryWidth = 0.04;
const float GapFar = 0.55;
const float WallGain = 0.35;

float Smooth(vec2 uv)
{
	return texture(Crust, uv).a;
}

void main()
{
	vec2 p = inWorld.xz;
	float time = inTime;
	float pulse = pc.Params.y;
	float fury = pc.Params.z;
	float pixel = max(length(dFdx(p)), length(dFdy(p)));

	vec2 bend = vec2(Smooth(p / 160.0 + 0.13), Smooth(p / 160.0 + 0.57)) - 0.5;
	vec2 q = p + bend * 34.0;
	float basin = Smooth(q / 120.0 + 0.29) * 0.65 + Smooth(q / 47.0 + 0.83) * 0.35;
	float ridge = 1.0 - abs(2.0 * Smooth(q / 72.0 + 0.71) - 1.0);
	float width = mix(0.05, 0.2, Smooth(p / 140.0 + 0.42));
	float river = smoothstep(1.0 - width * 3.2, 1.0 - width * 0.25, ridge);
	float ridge2 = 1.0 - abs(2.0 * Smooth(q / 29.0 + 0.15) - 1.0);
	river = max(river, smoothstep(1.0 - SecondaryWidth * 3.0, 1.0 - SecondaryWidth * 0.3, ridge2) * smoothstep(0.45, 0.62, Smooth(q / 85.0 + 0.5)) * 0.9);
	float pool = smoothstep(0.6, 0.72, basin);
	float edge = (Smooth(p / 9.0 + 0.17) - 0.5) * 0.16 + (texture(Crust, p / 3.1 + 0.47).b - 0.5) * 0.06;
	float heat = clamp(max(river, pool * 0.56 + smoothstep(0.76, 0.86, basin) * 0.44) + edge + FuryMelt * fury - pc.Params.w, 0.0, 1.0);
	float aa = max(pixel * 0.08, 0.004);
	float molten = smoothstep(MoltenLine - aa, MoltenLine + aa, heat);
	float skin = smoothstep(SkinLine - aa, SkinLine + aa, heat) * (1.0 - molten);
	float crust = 1.0 - molten - skin;

	vec2 moved = p - Drift * time;
	vec2 warp = texture(Crust, moved / (Period * 1.9) + time * vec2(0.0011, 0.0008)).ab - 0.5;
	vec2 along = normalize(Drift);
	vec2 flowPoint = vec2(dot(p, along), dot(p, vec2(-along.y, along.x)));
	float streak = texture(Crust, vec2((flowPoint.x - time * 0.16) / (Period * 0.55), flowPoint.y / (Period * 0.45)) + warp * 0.08).b;
	float swirl = texture(Crust, (moved + warp * 3.2) / (Period * 0.38) + time * vec2(-0.0021, 0.0017)).a;
	float churn = streak * 0.35 + swirl * 0.65;
	float core = smoothstep(MoltenLine, 1.0, heat);
	float heart = 0.86 + 0.28 * pulse;
	vec3 liquid = mix(Deep, Gold, smoothstep(0.35, 0.8, churn) * (0.4 + 0.6 * core));
	liquid = mix(liquid, WhiteHot, smoothstep(0.82, 0.97, churn) * core * 0.7);
	float film = smoothstep(0.6, 0.85, swirl) * (1.0 - core) * 0.45;
	liquid *= (1.0 - film) * heart;

	vec2 cellPoint = (moved + warp * 3.2) / BubbleScale;
	vec2 cellId = floor(cellPoint);
	vec3 random = fract(sin(vec3(dot(cellId, vec2(127.1, 311.7)), dot(cellId, vec2(269.5, 183.3)), dot(cellId, vec2(419.2, 371.9)))) * 43758.5453);
	float bubbleAge = fract(time * BubbleRate + random.z * 7.0);
	float bubbleRadius = BubbleSize * bubbleAge * (0.6 + 0.8 * random.x);
	float bubbleDistance = length(fract(cellPoint) - (0.3 + 0.4 * random.xy)) * BubbleScale;
	float popping = step(0.62, random.y) * molten;
	float bubble = (1.0 - smoothstep(bubbleRadius - pixel, bubbleRadius + pixel, bubbleDistance)) * popping * (1.0 - bubbleAge * 0.7);
	float ringLine = exp(-abs(bubbleDistance - bubbleRadius) / max(pixel, 0.015)) * popping * (1.0 - bubbleAge);
	liquid = mix(liquid, liquid * 0.4, bubble) + Gold * (ringLine * 0.6);
	vec4 raft = texture(Crust, (moved + warp * 3.2) / (Period * 0.8) + 0.27);
	float floating = molten * step(0.74, raft.g) * smoothstep(0.03, 0.09, raft.r) * (1.0 - core * 0.6);
	float raftRim = molten * step(0.74, raft.g) * (1.0 - smoothstep(0.0, 0.05, abs(raft.r - 0.06)));
	liquid = mix(liquid, Fissure * 0.015, floating) + Gold * raftRim * 0.35;

	vec2 cq = p + bend * 34.0 + (vec2(Smooth(p / 11.0 + 0.3), Smooth(p / 11.0 + 0.9)) - 0.5) * 5.0;
	cq += (vec2(texture(Crust, p / 4.3 + 0.21).a, texture(Crust, p / 4.3 + 0.64).a) - 0.5) * 0.9;
	float scale = mix(PlateLow, PlateHigh, Smooth(p / 60.0 + 0.66));
	vec4 plates = texture(Crust, cq / (Period * scale));
	float border = plates.r * scale;
	float gapWidth = mix(GapNarrow, GapWide, smoothstep(0.25, 0.8, Smooth(cq / 23.0 + 0.37))) * (0.65 + 0.7 * plates.b);
	float sunk = step(1.0 - PoolShare, plates.g) * smoothstep(0.4, 0.6, Smooth(cq / 31.0 + 0.71));
	float gapAA = max(pixel * 0.7, 0.004);
	float gap = max(1.0 - smoothstep(gapWidth - gapAA, gapWidth + gapAA, border), sunk);
	float bevel = smoothstep(gapWidth, gapWidth + PlateBevel, border) * (1.0 - sunk);
	float wall = (1.0 - smoothstep(gapWidth, gapWidth + WallGlow, border)) * (1.0 - gap);
	float gapCore = 1.0 - smoothstep(0.0, gapWidth * 0.9, border);
	float crackDistance = texture(Crust, cq / (Period * 0.55) + 0.61).r * 0.55;
	float crack = (1.0 - smoothstep(0.008 - pixel * 0.5, 0.008 + pixel * 0.5, crackDistance)) * smoothstep(0.62, 0.78, Smooth(cq / 7.0 + 0.13));
	float shore = smoothstep(0.15, SkinLine, heat);
	float fissure = crack * 0.7 * (0.5 + 0.5 * smoothstep(0.55, 0.85, plates.b));
	float hot = 0.75 + 0.25 * churn;
	float live = max(shore, smoothstep(0.62, 0.8, Smooth(cq / 37.0 + 0.2)));
	vec3 gapGlow = mix(Deep * 0.55, Gold, max(gapCore * live, sunk * 0.6)) * hot * mix(GapFar, 1.0, shore) * mix(GapCooled, 1.0, max(live, sunk));
	gapGlow = mix(gapGlow, liquid, sunk);

	float tone = 0.7 + 0.6 * plates.b;
	vec3 rock = mix(CrustColor, BasaltRed, smoothstep(0.4, 0.8, plates.g) * 0.7 + shore * 0.3) * tone;
	vec3 skinTone = SkinColor * (0.7 + 0.6 * plates.b);
	float dome = Smooth(cq / 2.7 + plates.g * 3.0) * 0.35;
	float plated = crust + skin;
	float lift = plated * bevel * CrustRelief * (0.65 + 0.35 * plates.b + dome);
	mat2 footprint = mat2(dFdx(p), dFdy(p));
	vec2 slope = abs(determinant(footprint)) > 1e-8 ? inverse(transpose(footprint)) * vec2(dFdx(lift), dFdy(lift)) : vec2(0.0);
	vec2 ripple = abs(determinant(footprint)) > 1e-8 ? inverse(transpose(footprint)) * vec2(dFdx(churn), dFdy(churn)) * 0.04 * molten : vec2(0.0);
	vec3 normal = normalize(vec3(-slope.x - ripple.x, 1.0, -slope.y - ripple.y));

	vec3 emission = liquid * molten;
	emission += (SkinGlow * (0.5 + 0.8 * swirl) * (1.0 - gap) + Fissure * crack * 0.6) * skin * heart;
	emission += Fissure * (fissure * crust * (1.0 - gap) * (0.14 + 0.1 * pulse));
	emission += (gapGlow * gap * GapGain + Fissure * wall * WallGain * live * (0.4 + 0.6 * shore)) * plated * heart;
	float fade = DepthFade(pc.Params.x, inWorld.y);
	vec3 albedo = (rock * crust + skinTone * skin) * (1.0 - gap) + Deep * 0.2 * plated * gap + CrustColor * floating;
	float roughness = molten * mix(0.14, 0.85, floating) + skin * 0.62 + crust * mix(mix(0.78, 0.94, plates.b), 0.3, gap);
	outAlbedo = vec4(albedo, GroundOcclusion(0.0));
	outSurface = PackSurface(normal, roughness, molten * 0.6);
	outEmission = vec4(emission * (1.0 - fade), PackEmissionTag(false, CrustTranslucency * skin * 0.5));
}
