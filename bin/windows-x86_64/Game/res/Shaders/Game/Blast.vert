#version 450
#include "Blast.glsl"

layout(std430, set = 0, binding = 0) readonly buffer BlastBuffer { BlastData Blasts[]; };

layout(location = 0) out vec3 outWorld;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outCoords;
layout(location = 3) flat out vec4 outInfo;
layout(location = 4) flat out vec4 outTimes;
layout(location = 5) flat out vec4 outCenter;

const ivec2 Corners[6] = ivec2[](ivec2(0, 0), ivec2(1, 0), ivec2(1, 1), ivec2(0, 0), ivec2(1, 1), ivec2(0, 1));

vec3 PuffPoint(uint local, vec3 center, vec2 radial, vec3 size, float salt, float churn, out vec3 normal, out vec2 uv, out float lump)
{
	uint quad = local / 6u;
	ivec2 corner = Corners[local % 6u];
	float a = float(quad % BlastPuffSegments + uint(corner.x)) / float(BlastPuffSegments);
	float b = float(quad / BlastPuffSegments + uint(corner.y)) / float(BlastPuffRings);
	float theta = BlastTau * a;
	float phi = BlastPi * b;
	vec3 dir = vec3(cos(theta) * sin(phi), cos(phi), sin(theta) * sin(phi));
	vec3 offset = vec3(salt * 1.37, salt * 0.71 - churn, salt * 2.13);
	float coarse = BlastNoise3(dir * 2.4 + offset);
	float detail = BlastNoise3(dir * 5.2 + offset * 1.3 + vec3(0.0, churn * 0.6, 0.0));
	lump = mix(0.62 * coarse + 0.38 * detail, 0.62 * abs(2.0 * coarse - 1.0) + 0.38 * abs(2.0 * detail - 1.0), 0.45);
	vec3 away = vec3(radial.x, 0.0, radial.y);
	vec3 side = vec3(-radial.y, 0.0, radial.x);
	vec3 scaled = dir / max(size, vec3(1e-3));
	normal = normalize(away * scaled.x + vec3(0.0, scaled.y, 0.0) + side * scaled.z);
	uv = vec2(a, b);
	vec3 shaped = dir * size * (1.0 + 0.5 * (lump - 0.5));
	return center + away * shaped.x + vec3(0.0, shaped.y, 0.0) + side * shaped.z;
}

void main()
{
	BlastData blast = Blasts[gl_InstanceIndex];
	vec3 ground = blast.GroundRadius.xyz;
	float radius = blast.GroundRadius.w;
	float t = blast.Look.x;
	float spin = blast.Look.w;
	float violet = blast.Look.z;
	float power = blast.Look.y;
	float seed = blast.Shape.y;
	float smoke = blast.Shape.z;
	float twist = spin * 0.26 * (1.0 - (1.0 - min(t, 1.0)) * (1.0 - min(t, 1.0)));
	uint index = uint(gl_VertexIndex);
	bool fire = index < BlastJetEnd;
	if ((fire && t >= 1.0) || (!fire && smoke >= 1.0))
	{
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
		return;
	}
	outTimes = vec4(t, smoke, blast.Shape.w, spin);
	outCenter = vec4(ground, radius);
	vec3 position;
	vec3 normal;
	if (index < BlastDomeEnd)
	{
		uint quad = index / 6u;
		ivec2 corner = Corners[index % 6u];
		float a = float(quad % BlastDomeSegments + uint(corner.x)) / float(BlastDomeSegments);
		float s = float(quad / BlastDomeSegments + uint(corner.y)) / float(BlastDomeRings);
		float grow = BlastEaseOut(t / 0.18);
		float slump = smoothstep(0.3, 1.0, t);
		float domeRadius = radius * (0.08 + 0.5 * grow + 0.05 * slump) * mix(1.1, 1.0, power);
		float domeHeight = radius * blast.Shape.x * (0.3 + 0.7 * grow) * (1.0 - 0.25 * slump) * mix(1.15, 1.0, power);
		float theta = BlastTau * a;
		float phi = s * (0.5 * BlastPi + 0.55);
		float turned = a - twist * (1.0 + 0.5 * (1.0 - s)) / BlastTau;
		float lump = 0.65 * BlastLoopNoise(vec2(turned * 9.0 + s * 2.0, s * 3.0 - t * 1.2 + seed), 9.0) + 0.35 * BlastLoopNoise(vec2(turned * 19.0 + s * 4.0, s * 6.0 - t * 2.0 + seed * 1.7), 19.0);
		float bulge = 1.0 + (0.06 + 0.1 * smoothstep(0.2, 0.9, s)) * (lump - 0.5);
		vec2 around = vec2(cos(theta), sin(theta));
		float across = domeRadius * sin(phi) * bulge;
		float up = domeHeight * (cos(phi) + 0.3) * (1.0 + 0.1 * (lump - 0.5));
		position = ground + vec3(around.x * across, up, around.y * across);
		normal = normalize(vec3(around.x * sin(phi) / max(domeRadius, 1e-3), cos(phi) / max(domeHeight, 1e-3), around.y * sin(phi) / max(domeRadius, 1e-3)));
		outCoords = vec4(turned, s, 1.04 - 0.72 * s, lump);
		outInfo = vec4(BlastDome, t, violet, seed);
	}
	else if (index < BlastFireEnd)
	{
		uint local = index - BlastDomeEnd;
		uint puff = local / BlastPuffVertices;
		bool outer = puff < BlastOuterPuffs;
		float k = outer ? float(puff) : float(puff - BlastOuterPuffs);
		float count = floor(outer ? mix(6.0, float(BlastOuterPuffs), power) + 0.5 : mix(3.0, float(BlastFirePuffs - BlastOuterPuffs), power) + 0.5);
		if (k >= count)
		{
			gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
			return;
		}
		float h1 = BlastHash(vec2(float(puff) * 5.17 + seed, 2.3));
		float h2 = BlastHash(vec2(float(puff) * 1.91 + 7.0, seed + 0.7));
		float h3 = BlastHash(vec2(float(puff) * 3.33 + seed * 0.5, 9.1));
		float h4 = BlastHash(vec2(float(puff) * 8.71, seed * 1.9 + 4.4));
		float expand = BlastEaseOut((t - 0.03 * h3) / (0.26 + 0.1 * h4));
		float reach = outer ? 0.5 + 0.18 * h1 : 0.24 + 0.2 * h1;
		float distance = radius * mix(0.08, reach, expand);
		float size = radius * (outer ? 0.12 + 0.07 * h3 : 0.13 + 0.07 * h3) * (0.35 + 0.65 * expand) * (1.0 + 0.3 * smoothstep(0.3, 1.0, t)) * mix(1.25, 1.0, power);
		float smear = 1.0 + 1.4 * (1.0 - expand) * (1.0 - expand);
		float angle = BlastTau * (k + 0.6 * h2) / count + twist * (1.25 - 0.35 * reach);
		vec2 radial = vec2(cos(angle), sin(angle));
		vec3 center = ground + vec3(radial.x * distance, size * 0.42 + radius * 0.02, radial.y * distance);
		vec2 uv;
		float lump;
		position = PuffPoint(local % BlastPuffVertices, center, radial, vec3(size * smear, size * 0.68, size * (outer ? 1.75 : 1.2)), seed + float(puff) * 3.1, t * 1.5, normal, uv, lump);
		float core = 1.0 - clamp(length(position.xz - ground.xz) / radius, 0.0, 1.0);
		float rise = clamp((position.y - ground.y) / (radius * 0.35), 0.0, 1.0);
		outCoords = vec4(h4, uv.y, 0.3 + 0.46 * core + 0.12 * rise + 0.1 * h4, lump);
		outInfo = vec4(BlastPuff, t, violet, seed);
	}
	else if (index < BlastJetEnd)
	{
		uint local = index - BlastFireEnd;
		uint jet = local / (BlastJetSegments * 6u);
		float count = floor(mix(10.0, float(BlastJets), power) + 0.5);
		if (float(jet) >= count)
		{
			gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
			return;
		}
		uint quad = (local / 6u) % BlastJetSegments;
		ivec2 corner = Corners[local % 6u];
		float u = float(quad + uint(corner.x)) / float(BlastJetSegments);
		float v = float(corner.y);
		float h1 = BlastHash(vec2(float(jet) * 7.13 + seed, seed * 0.37 + 1.7));
		float h2 = BlastHash(vec2(float(jet) * 3.71 + 11.0, seed + 5.3));
		float h3 = BlastHash(vec2(float(jet) * 5.29 + seed * 0.6, 8.8));
		float h4 = BlastHash(vec2(float(jet) * 2.17 + seed * 1.3, 4.6));
		float h5 = BlastHash(vec2(float(jet) * 8.31, seed * 0.9 + 3.1));
		bool low = h4 < 0.55;
		float sweep = spin * (h5 < 0.2 ? -0.6 : 1.0);
		float outward = BlastHash(vec2(float(jet) * 6.43 + seed * 0.2, 2.9));
		float start = BlastTau * (float(jet) + 0.65 * h1) / count;
		float span = ((low ? 0.65 : 0.5) + 0.35 * h2) * (1.0 - 0.35 * outward);
		float whip = BlastEaseOut((t - 0.03 * h3) / (0.16 + 0.04 * h2));
		float ring = radius * mix(0.15, low ? 0.93 + 0.16 * h1 : 0.56 + 0.2 * h1, whip);
		float angle = start + twist * 1.3 + sweep * span * (u - 0.5) + spin * (0.2 + 0.1 * h3) * t;
		float curl = ring * (1.0 - (0.35 + 0.15 * outward) * (1.0 - u) * (1.0 - u));
		float width = radius * (low ? 0.24 : 0.2) * (0.8 + 0.4 * h2) * (0.5 + 0.5 * whip);
		width *= pow(max(sin(BlastPi * pow(u, 1.35)), 0.0), 0.6);
		float lean = low ? mix(0.7, 0.35, t) : mix(1.4, 0.8, t);
		float lift = low ? 0.04 : radius * (0.1 + 0.08 * h3);
		vec2 radial = vec2(cos(angle), sin(angle));
		float distance = curl + (v - 0.5) * width;
		position = ground + vec3(radial.x * distance, lift + (1.0 - v) * width * lean, radial.y * distance);
		normal = normalize(vec3(radial.x * lean, 1.0, radial.y * lean));
		outCoords = vec4(u, v, 0.95 - 0.4 * v - 0.2 * abs(u - 0.6), float(jet) * 1.618 + seed);
		outInfo = vec4(BlastJet, t, violet, seed);
	}
	else
	{
		uint local = index - BlastJetEnd;
		uint puff = local / BlastPuffVertices;
		float h1 = BlastHash(vec2(float(puff) * 4.21 + seed, 6.1));
		float h2 = BlastHash(vec2(float(puff) * 2.63 + 3.0, seed + 8.2));
		float h3 = BlastHash(vec2(float(puff) * 6.07 + seed * 0.3, 1.4));
		float h4 = BlastHash(vec2(float(puff) * 9.89, seed * 1.1 + 2.2));
		float born = 0.22 + 0.12 * h1;
		float count = floor(mix(6.0, float(BlastSmokePuffs), power) + 0.5);
		if (smoke < born || float(puff) >= count)
		{
			gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
			return;
		}
		float life = (smoke - born) / (1.0 - born);
		float angle = BlastTau * (float(puff) + 0.5 * h2) / count + spin * 0.35 * BlastEaseOut(smoke);
		float distance = radius * mix(0.55, 0.8 + 0.2 * h3, BlastEaseOut(life / 0.6));
		float size = radius * (0.15 + 0.07 * h4) * (0.55 + 0.6 * BlastEaseOut(life / 0.7));
		vec2 radial = vec2(cos(angle), sin(angle));
		vec3 center = ground + vec3(radial.x * distance, size * 0.5 + radius * 0.08 * life, radial.y * distance);
		vec2 uv;
		float lump;
		position = PuffPoint(local % BlastPuffVertices, center, radial, vec3(size, size * 0.75, size * 1.35), seed + float(puff) * 5.7 + 40.0, smoke * 1.2, normal, uv, lump);
		float salt = seed + float(puff) * 7.3;
		float grain = BlastNoise3(normal * 3.5 + vec3(salt * 0.37, salt * 0.11 - smoke * 1.5, salt * 0.53));
		outCoords = vec4(uv, life, 0.55 * lump + 0.45 * grain);
		outInfo = vec4(BlastSmoke, smoke, violet, seed + float(puff) * 7.3);
	}
	outWorld = position;
	outNormal = normal;
	gl_Position = pc.ViewProjection * vec4(position, 1.0);
}
