#version 450

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Constants
{
	vec2 Resolution;
	vec2 Mouse;
	vec4 Tint;
	float Time;
	float Focus;
	float Fade;
	float Quality;
	float Angle;
	float Fracture;
	float Pulse;
	float Sides;
	float Meet;
	float Pad0;
	float Pad1;
	float Pad2;
} pc;

const float Pi = 3.14159265;
const float PortalRadius = 0.8;
const float Sectors = 11.0;
const float Spiral = Sectors / 6.2831853;
const float Softening = 0.16;

struct Shard
{
	vec2 Cell;
	vec2 Center;
	vec2 EdgeDirection;
	float Edge;
	float Seed;
	float Radius;
};

float Hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

vec2 Hash22(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.103, 0.0973));
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.xx + p3.yz) * p3.zy);
}

vec4 Hash42(vec2 p)
{
	vec4 p4 = fract(vec4(p.xyxy) * vec4(0.1031, 0.103, 0.0973, 0.1099));
	p4 += dot(p4, p4.wzxy + 33.33);
	return fract((p4.xxyz + p4.yzzw) * p4.zywx);
}

float Noise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(Hash12(i), Hash12(i + vec2(1.0, 0.0)), f.x), mix(Hash12(i + vec2(0.0, 1.0)), Hash12(i + vec2(1.0, 1.0)), f.x), f.y);
}

float Fbm(vec2 p)
{
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 4; i++)
	{
		value += amplitude * Noise(p);
		p = p * 2.03 + vec2(11.3, 5.7);
		amplitude *= 0.5;
	}
	return value / 0.9375;
}

vec2 Rotate(vec2 v, float angle)
{
	float s = sin(angle);
	float c = cos(angle);
	return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

vec2 ComplexMultiply(vec2 a, vec2 b)
{
	return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

vec2 ComplexDivide(vec2 a, vec2 b)
{
	return vec2(a.x * b.x + a.y * b.y, a.y * b.x - a.x * b.y) / dot(b, b);
}

vec2 Wrap(vec2 cell)
{
	return vec2(cell.x, mod(cell.y, Sectors));
}

vec2 SeedPoint(vec2 cell)
{
	return 0.5 + 0.38 * sin(pc.Time * 0.04 + 6.2831853 * Hash22(Wrap(cell)));
}

Shard Fracture(vec2 p, vec2 center)
{
	vec2 d = p - center;
	float r = sqrt(dot(d, d) + Softening);
	float spin = pc.Time * 0.008;
	float angle = atan(d.y, d.x) + spin;
	vec2 q = vec2(log(r), angle) * Spiral;
	vec2 base = floor(q);
	vec2 f = q - base;
	float best = 1e9;
	vec2 bestCell = base;
	vec2 bestOffset = vec2(0.0);
	for (int y = -1; y <= 1; y++)
		for (int x = -1; x <= 1; x++)
		{
			vec2 cell = base + vec2(x, y);
			vec2 offset = vec2(x, y) + SeedPoint(cell) - f;
			float distance = dot(offset, offset);
			if (distance < best)
			{
				best = distance;
				bestCell = cell;
				bestOffset = offset;
			}
		}
	float edge = 1e9;
	vec2 edgeCell = bestCell;
	vec2 edgeNormal = vec2(1.0, 0.0);
	vec2 near = bestCell - base;
	for (int y = -1; y <= 1; y++)
		for (int x = -1; x <= 1; x++)
		{
			vec2 cell = bestCell + vec2(x, y);
			vec2 offset = near + vec2(x, y) + SeedPoint(cell) - f;
			vec2 between = offset - bestOffset;
			if (dot(between, between) > 1e-6)
			{
				vec2 normal = normalize(between);
				float distance = dot(0.5 * (bestOffset + offset), normal);
				if (distance < edge)
				{
					edge = distance;
					edgeCell = cell;
					edgeNormal = normal;
				}
			}
		}
	vec2 site = (q + bestOffset) / Spiral;
	float radius = sqrt(max(exp(2.0 * site.x) - Softening, 0.0));
	vec2 radial = vec2(cos(angle - spin), sin(angle - spin));
	Shard shard;
	shard.Cell = Wrap(bestCell);
	shard.Center = center + radius * vec2(cos(site.y - spin), sin(site.y - spin));
	shard.EdgeDirection = normalize(edgeNormal.x * radial + edgeNormal.y * vec2(-radial.y, radial.x));
	shard.Edge = edge * r / Spiral;
	shard.Seed = Hash12(Wrap(bestCell) + Wrap(edgeCell) + 0.5);
	shard.Radius = radius;
	return shard;
}

float Stars(vec2 p, float density, float seed)
{
	vec2 cell = floor(p * density);
	float h = Hash12(cell + seed);
	if (h < 0.955)
		return 0.0;
	vec2 offset = Hash22(cell + seed + 5.1) - 0.5;
	float d = length(fract(p * density) - 0.5 - offset * 0.55);
	float twinkle = 0.55 + 0.45 * sin(pc.Time * (0.7 + 2.2 * h) + h * 60.0);
	return smoothstep(0.17, 0.0, d) * twinkle * (0.3 + 0.7 * (h - 0.955) / 0.045);
}

vec3 Space(vec2 p)
{
	float drift = pc.Time * 0.01;
	float n = Fbm(p * 1.1 + vec2(drift, -0.4 * drift));
	float m = Fbm(p * 2.2 + n * 1.7 - vec2(0.0, drift));
	vec3 color = mix(vec3(0.005, 0.004, 0.014), vec3(0.018, 0.012, 0.04), smoothstep(-1.2, 1.0, p.y));
	color += vec3(0.19, 0.06, 0.36) * pow(m, 3.4) * 0.36;
	color += vec3(0.02, 0.15, 0.2) * pow(n, 4.5) * 0.45;
	color += vec3(0.75, 0.8, 1.0) * Stars(p + pc.Mouse * 0.012, 34.0, 0.37);
	color += vec3(0.9, 0.78, 1.0) * Stars(p * 0.62 + pc.Mouse * 0.006, 34.0, 9.21) * 0.6;
	return color;
}

vec3 Portal(vec2 z, vec3 tint, float pixels, float phase)
{
	float a = Pi / max(pc.Sides, 3.0);
	float b = Pi / max(pc.Meet, 3.0);
	float k = sqrt(max(cos(b) * cos(b) - sin(a) * sin(a), 1e-4));
	vec2 mirror = vec2(cos(b) / k, 0.0);
	float mirrorRadius2 = sin(a) * sin(a) / (k * k);
	vec2 normal = vec2(-sin(a), cos(a));
	float inradius = mirror.x - sin(a) / k;
	float scale = 1.0;

	z = Rotate(z, pc.Angle + pc.Time * 0.02 + phase);
	vec2 shift = pc.Mouse * vec2(0.32, -0.32);
	vec2 denominator = vec2(1.0, 0.0) - ComplexMultiply(vec2(shift.x, -shift.y), z);
	scale *= (1.0 - dot(shift, shift)) / dot(denominator, denominator);
	z = ComplexDivide(z - shift, denominator);
	float travel = mod(pc.Time * 0.1 + phase * 3.0, 4.0 * atanh(min(inradius, 0.999)));
	float t = tanh(0.5 * travel);
	denominator = vec2(1.0, 0.0) + t * z;
	scale *= (1.0 - t * t) / dot(denominator, denominator);
	z = ComplexDivide(z + vec2(t, 0.0), denominator);

	float depth = 0.0;
	float parity = 0.0;
	for (int i = 0; i < 48; i++)
	{
		bool settled = true;
		if (z.y < 0.0)
		{
			z.y = -z.y;
			parity = 1.0 - parity;
			settled = false;
		}
		float side = dot(z, normal);
		if (side > 0.0)
		{
			z -= 2.0 * side * normal;
			parity = 1.0 - parity;
			settled = false;
		}
		vec2 w = z - mirror;
		float ww = dot(w, w);
		if (ww < mirrorRadius2)
		{
			float factor = mirrorRadius2 / ww;
			z = mirror + w * factor;
			scale *= factor;
			depth += 1.0;
			settled = false;
		}
		if (settled)
			break;
	}

	float units = pixels / scale;
	float tile = inradius * units;
	float detail = smoothstep(2.0, 9.0, tile);
	float edge = (length(z - mirror) - sqrt(mirrorRadius2)) * units;
	float symmetry = min(z.y, -dot(z, normal)) * units;
	float star = length(z) * units;
	float ring = abs(length(z) - inradius * 0.62) * units;
	float pulse = 0.55 + 0.45 * sin(pc.Time * 0.9 - depth * 1.15);
	float twinkle = 0.6 + 0.4 * sin(pc.Time * 1.7 + depth * 2.3 + parity * 1.7);

	vec3 color = mix(vec3(0.006, 0.004, 0.016), tint * 0.025, parity * detail);
	color += tint * (exp(-edge * 0.85) * 0.85 + exp(-edge / 6.0) * 0.16) * detail * pulse;
	color += mix(tint, vec3(1.0), 0.3) * exp(-symmetry * 1.5) * 0.06 * detail;
	color += tint * exp(-ring * 1.1) * 0.28 * detail * twinkle;
	color += mix(tint, vec3(1.0), 0.75) * exp(-star / max(tile * 0.05, 0.8)) * detail * twinkle;
	color += tint * (1.0 - detail) * 0.32;
	return color;
}

vec3 Scene(vec2 view, vec2 center, vec3 tint, float ndcPixel, float reality)
{
	vec2 relative = view - center;
	float r = length(relative);
	float rim = r - PortalRadius;
	vec3 color = vec3(0.0);
	if (rim > -2.0 * ndcPixel)
	{
		float bent = r - 0.62 * PortalRadius * PortalRadius / max(r, PortalRadius);
		color = Space(center + relative / max(r, 1e-4) * bent + pc.Mouse * 0.03);
	}
	if (rim < 2.0 * ndcPixel)
	{
		vec3 inside = Portal(relative / PortalRadius, mix(tint, vec3(1.0, 0.66, 0.3), reality), PortalRadius / ndcPixel, reality * 0.37);
		color = mix(color, inside, smoothstep(ndcPixel, -ndcPixel, rim));
	}
	color += mix(tint, vec3(1.0), 0.6) * exp(-abs(rim) / (ndcPixel * 2.2)) * 0.9;
	color += tint * exp(-abs(rim) / 0.07) * 0.22;
	return color;
}

void main()
{
	vec2 frag = inUv * pc.Resolution;
	float aspect = pc.Resolution.x / pc.Resolution.y;
	vec2 p = vec2((inUv.x * 2.0 - 1.0) * aspect, 1.0 - inUv.y * 2.0);
	float ndcPixel = 2.0 / pc.Resolution.y;
	vec2 center = vec2(pc.Focus, 0.0) + pc.Mouse * 0.045;
	vec3 tint = pc.Tint.rgb;

	vec2 outward = p - center;
	float reach = length(outward);
	float wave = exp(-pow((reach - pc.Pulse * 1.6) / 0.22, 2.0)) * exp(-pc.Pulse * 1.2);
	vec2 warped = p + outward / max(reach, 1e-3) * wave * 0.07;

	Shard shard = Fracture(warped, center);
	float strength = pc.Fracture * smoothstep(1.9, 0.35, shard.Radius) * smoothstep(0.25, 0.6, shard.Radius);
	vec4 h = Hash42(shard.Cell);
	vec2 radial = normalize(shard.Center - center + vec2(1e-4, 0.0));
	vec2 tilt = Hash22(shard.Cell + 3.0) * 2.0 - 1.0;
	float depth = 0.3 + 0.7 * h.x;
	vec2 local = Rotate(warped - shard.Center, (h.y - 0.5) * 0.22 * strength) * (1.0 + (h.z - 0.5) * 0.26 * strength);
	vec2 shift = (radial * (h.z - 0.35) * 0.08 + tilt * 0.05 + pc.Mouse * 0.07 * (depth - 0.5)) * strength;
	float edge = shard.Edge / ndcPixel;
	float bevel = exp(-edge / 7.0) * strength;
	vec2 view = shard.Center + local + shift - shard.EdgeDirection * bevel * 0.04;
	float reality = step(0.9, Hash12(shard.Cell + 7.7)) * smoothstep(0.2, 0.6, strength);

	vec3 color;
	if (bevel > 0.05)
	{
		vec2 spread = shard.EdgeDirection * bevel * 0.014;
		color = vec3(Scene(view - spread, center, tint, ndcPixel, reality).r, Scene(view, center, tint, ndcPixel, reality).g, Scene(view + spread, center, tint, ndcPixel, reality).b);
	}
	else
		color = Scene(view, center, tint, ndcPixel, reality);
	color *= mix(1.0, 1.0 - 0.45 * reality, strength);
	color += vec3(1.0) * exp(-pc.Pulse * 5.0) * 0.35 * smoothstep(PortalRadius + 0.1, PortalRadius - 0.1, length(view - center));

	vec2 facing = normalize(tilt + vec2(1e-4, 0.0));
	float sheen = smoothstep(-0.5, 0.5, dot(warped - shard.Center, facing) * 2.0 + dot(pc.Mouse, facing) * 0.6);
	color *= mix(1.0, 0.85 + 0.3 * h.y, strength);
	color += vec3(0.55, 0.5, 1.0) * sheen * 0.035 * strength;
	vec2 light = normalize(vec2(-0.6, 0.8) + pc.Mouse * 0.5);
	float lit = dot(-shard.EdgeDirection, light);
	color *= 1.0 - 0.3 * exp(-edge / 2.5) * strength;
	color += mix(tint, vec3(1.0), 0.7) * max(lit, 0.0) * exp(-edge / 3.0) * 0.22 * strength;
	float level = mix(0.05, 1.0, smoothstep(0.6, 0.82, shard.Seed));
	float glimmer = pow(0.5 + 0.5 * sin(reach * 9.0 - pc.Time * 1.6 + shard.Seed * 40.0), 6.0);
	vec3 crack = mix(tint, vec3(0.8, 0.95, 1.0), 0.55);
	color += crack * (exp(-edge * 0.85) * 0.6 + exp(-edge / 7.0) * 0.08) * strength * level * (0.45 + 1.4 * glimmer) * (1.0 + wave * 6.0);
	color += tint * wave * 0.05;

	color *= pc.Fade;
	color *= mix(0.55, 1.0, smoothstep(2.0, 0.4, length(p * vec2(0.7, 1.0))));
	color = 1.0 - exp(-color * 1.25);
	color = pow(color, vec3(1.0 / 2.2));
	color += (Hash12(frag + fract(pc.Time) * 97.0) - 0.5) / 255.0;
	outColor = vec4(color, 1.0);
}
