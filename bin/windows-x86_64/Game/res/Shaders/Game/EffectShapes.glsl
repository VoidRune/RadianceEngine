const uint ShapeOrb = 8u;
const uint ShapeLightning = 9u;
const uint ShapeRune = 10u;
const uint ShapeSlash = 11u;
const uint ShapeSnowflake = 12u;
const uint ShapeBeam = 13u;
const uint ShapeInferno = 14u;
const uint ShapeHex = 15u;

const uint DecalRunes = 0u;
const uint DecalSector = 1u;
const uint DecalLane = 2u;
const uint DecalRing = 3u;
const uint DecalGlow = 4u;
const uint DecalScorch = 5u;
const uint DecalFrost = 6u;
const uint DecalCracks = 7u;
const uint DecalFracture = 8u;
const uint DecalCharge = 9u;
const uint DecalFrostLane = 10u;
const uint DecalFissure = 11u;

const float Tau = 6.28318531;

float HashUnit(uint x)
{
	return float(Hash(x) & 0xFFFFu) / 65535.0;
}

float ValueNoise(vec2 p)
{
	vec2 cell = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	uvec2 c = uvec2(ivec2(cell) + 4096);
	float a = float(Hash(c.x * 1973u + c.y * 9277u) & 0xFFFFu) / 65535.0;
	float b = float(Hash((c.x + 1u) * 1973u + c.y * 9277u) & 0xFFFFu) / 65535.0;
	float d = float(Hash(c.x * 1973u + (c.y + 1u) * 9277u) & 0xFFFFu) / 65535.0;
	float e = float(Hash((c.x + 1u) * 1973u + (c.y + 1u) * 9277u) & 0xFFFFu) / 65535.0;
	return mix(mix(a, b, f.x), mix(d, e, f.x), f.y);
}

float Fbm(vec2 p)
{
	return ValueNoise(p) * 0.55 + ValueNoise(p * 2.03 + 17.1) * 0.3 + ValueNoise(p * 4.11 + 31.7) * 0.15;
}

vec2 Rotate(vec2 p, float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float SegmentDistance(vec2 p, vec2 a, vec2 b)
{
	vec2 pa = p - a;
	vec2 ba = b - a;
	float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-8), 0.0, 1.0);
	return length(pa - ba * h);
}

float Line(float distance, float width, float pixel)
{
	return 1.0 - smoothstep(width - pixel, width + pixel, distance);
}

float GlyphDistance(vec2 p, uint seed)
{
	uint h = Hash(seed * 2654435761u + 7u);
	float best = (h & 3u) != 0u ? SegmentDistance(p, vec2(0.0, -0.85), vec2(0.0, 0.85)) : 1e3;
	for (uint k = 0u; k < 3u; k++)
	{
		uint s = Hash(seed * 747796405u + k * 2891336453u + 1u);
		uint a = s % 9u;
		uint b = (s >> 9) % 9u;
		b = a == b ? (b + 4u) % 9u : b;
		vec2 pa = (vec2(float(a % 3u), float(a / 3u)) - 1.0) * vec2(0.6, 0.85);
		vec2 pb = (vec2(float(b % 3u), float(b / 3u)) - 1.0) * vec2(0.6, 0.85);
		best = min(best, SegmentDistance(p, pa, pb));
	}
	return best;
}

float RuneCircle(vec2 uv, float pixel, float radius, float progress, float appear, float inner, float spin, uint seed, float time)
{
	float r = length(uv);
	float angle = atan(uv.y, uv.x);
	float turn = fract(angle / Tau + 0.25);
	float reveal = clamp((appear * 1.06 - turn) / 0.06, 0.0, 1.0) * smoothstep(0.0, 0.25, appear);
	float width = max(0.024 / radius, pixel * 0.6);
	float rotation = time * spin;
	float outer = 0.965;
	float glow = Line(abs(r - outer), width * 1.4, pixel) + 0.55 * Line(abs(r - (outer - width * 4.5)), width * 0.7, pixel);

	float band = clamp(0.3 / radius, 0.07, 0.24);
	float bandTop = outer - width * 8.0;
	float bandMid = bandTop - band * 0.5;
	float cells = max(5.0, floor(Tau * bandMid / (band * 0.9)));
	float spun = fract((angle + rotation) / Tau) * cells;
	float glyphScale = band * 0.42;
	vec2 glyphPoint = vec2((fract(spun) - 0.5) * Tau * bandMid / cells, r - bandMid) / glyphScale;
	float glyphs = Line(GlyphDistance(glyphPoint, uint(floor(spun)) + seed * 131u), width / glyphScale, pixel / glyphScale) * step(abs(glyphPoint.y), 1.2);
	glow += 0.75 * glyphs + 0.5 * Line(abs(r - (bandTop - band)), width * 0.7, pixel);

	if (inner <= 0.0)
	{
		float tickRing = bandTop - band - width * 5.0;
		float tickTurn = fract((angle - rotation * 0.7) / Tau * 64.0);
		float ticks = Line(abs(tickTurn - 0.5) * Tau * tickRing / 64.0, width * 0.6, pixel) * step(tickRing - 0.045, r) * step(r, tickRing);
		glow += 0.55 * ticks + 0.6 * Line(abs(r - (tickRing - 0.05)), width * 0.7, pixel);
		uint points = 5u + seed % 4u;
		uint skip = points >= 7u ? 3u : 2u;
		float starRadius = tickRing - 0.09;
		float star = 1e3;
		for (uint i = 0u; i < 8u; i++)
		{
			if (i >= points)
				break;
			float a0 = Tau * float(i) / float(points) - rotation * 0.4;
			float a1 = Tau * float((i + skip) % points) / float(points) - rotation * 0.4;
			star = min(star, SegmentDistance(uv, starRadius * vec2(cos(a0), sin(a0)), starRadius * vec2(cos(a1), sin(a1))));
		}
		glow += 0.65 * Line(star, width * 0.75, pixel) + 0.5 * Line(abs(r - starRadius), width * 0.6, pixel);
		glow += 0.6 * Line(abs(r - starRadius * 0.3), width * 0.7, pixel) + exp(-r * r / max(0.0015, width * width * 4.0));
	}
	else
		glow += Line(abs(r - inner), width * 1.4, pixel) + 0.55 * Line(abs(r - (inner + width * 4.5)), width * 0.7, pixel);

	float area = step(r, outer) * step(inner, r);
	float fill = 0.03 * area;
	if (progress >= 0.0)
	{
		float front = mix(inner, outer, progress);
		fill += area * step(r, front) * 0.07 + 0.85 * Line(abs(r - front), width * 1.6, pixel) * step(inner, r);
		glow *= 0.75 + 0.5 * progress;
	}
	return (glow + fill) * reveal;
}

float SectorShape(vec2 uv, float pixel, float radius, float halfArc, float progress, float appear)
{
	float r = length(uv);
	float angle = abs(atan(uv.y, uv.x));
	float width = max(0.03 / radius, pixel * 0.6);
	float inArc = step(angle, halfArc);
	float reveal = clamp((appear * 1.1 - r) / 0.1, 0.0, 1.0);
	float sides = Line(r * sin(min(abs(angle - halfArc), 1.5)), width, pixel) * step(r, 0.97);
	float rim = Line(abs(r - 0.965), width * 1.4, pixel) * inArc;
	float rings = 0.4 * (Line(abs(r - 0.36), width * 0.6, pixel) + Line(abs(r - 0.68), width * 0.6, pixel)) * inArc;
	float spacing = 0.35 / radius;
	float tick = Line(abs(fract(angle * r / spacing) - 0.5) * spacing, width * 0.6, pixel) * step(0.9, r) * step(r, 0.95) * inArc;
	float area = inArc * step(r, 0.965);
	float fill = 0.035 * area;
	if (progress >= 0.0)
	{
		float front = 0.965 * progress;
		fill += area * step(r, front) * 0.08 + 0.85 * Line(abs(r - front), width * 1.8, pixel) * inArc;
	}
	return (rim + sides + rings + 0.5 * tick + fill) * reveal;
}

float LaneShape(vec2 uv, vec2 extent, float pixel, float progress, float appear, float time)
{
	vec2 p = uv * extent;
	float width = max(0.03, pixel * 0.6);
	float across = step(abs(p.y), extent.y);
	float along = step(abs(p.x), extent.x);
	float inside = across * along;
	float border = Line(extent.y - abs(p.y), width, pixel) * along;
	float cap = Line(abs(p.x - (extent.x - width * 1.5)), width * 1.4, pixel) * across;
	float start = 0.5 * Line(abs(p.x + extent.x - width), width, pixel) * across;
	float chevron = fract((p.x + abs(p.y) * 0.7) / 0.8 - time * 1.4);
	float chevrons = 0.22 * Line(abs(chevron - 0.5) * 0.8, 0.05, pixel) * step(abs(p.y), extent.y * 0.8) * along;
	float fill = 0.035 * inside;
	float reveal = clamp((appear * 2.2 * extent.x - (p.x + extent.x)) / 0.3, 0.0, 1.0);
	if (progress >= 0.0)
	{
		float front = extent.x * (2.0 * progress - 1.0);
		fill += inside * step(p.x, front) * 0.08 + 0.85 * Line(abs(p.x - front), width * 1.8, pixel) * across;
	}
	return (border + cap + start + chevrons + fill) * reveal;
}

float RingGlow(float r, float center, float width, float pixel)
{
	float halo = (r - center) / (width * 4.0 + 0.01);
	return (Line(abs(r - center), width, pixel) + 0.3 * exp(-halo * halo)) * (1.0 - smoothstep(0.85, 1.0, r));
}

float RingShape(vec2 uv, float pixel, float radius, float thickness, float center, float dashes, float spin, float time, uint pattern)
{
	float r = length(uv);
	float width = max(thickness / radius, pixel * 0.6);
	float turn = fract(atan(uv.y, uv.x) / Tau + time * spin);
	float window = 1.0 - smoothstep(0.85, 1.0, r);
	float ring = pattern == 2u ? RingGlow(r, center - width * 1.7, width * 0.65, pixel) + RingGlow(r, center + width * 1.7, width * 0.65, pixel) : RingGlow(r, center, width, pixel);
	if (pattern == 1u)
	{
		float dash = fract(turn * 6.0);
		ring *= smoothstep(0.06, 0.14, dash) * (1.0 - smoothstep(0.62, 0.7, dash));
	}
	else if (pattern == 3u)
	{
		float along = abs(fract(turn * 12.0 + 0.5) - 0.5) * Tau * center / 12.0;
		float d = length(vec2(along, r - center));
		ring = (Line(d, width * 1.9, pixel) + 0.35 * exp(-d * d / (width * width * 9.0))) * window;
	}
	else if (pattern == 4u || pattern == 5u)
	{
		float count = pattern == 4u ? 3.0 : 4.0;
		float along = abs(fract(turn * count + 0.5) - 0.5) * Tau * r / count;
		float reach = width * (pattern == 4u ? 6.5 : 5.0);
		float spoke = pattern == 4u ? r - center : center - r;
		float shape = pattern == 4u ? Line(along, width * 1.25, pixel) : Line(along, width * 2.6 * (1.0 - clamp(spoke / reach, 0.0, 1.0)), pixel);
		ring = max(ring, shape * step(0.0, spoke) * (1.0 - smoothstep(reach * 0.8, reach, spoke)) * window);
	}
	if (dashes > 0.0)
	{
		float dash = fract(turn * dashes);
		ring *= smoothstep(0.1, 0.2, dash) * (1.0 - smoothstep(0.7, 0.8, dash));
	}
	return ring;
}

vec2 ScorchShape(vec2 uv, float heat, uint seed)
{
	float r = length(uv);
	vec2 offset = vec2(float(seed & 255u), float((seed >> 8) & 255u)) * 0.731;
	float n = Fbm(uv * 2.6 + offset);
	float edge = (1.0 - smoothstep(0.5, 0.98, r + (n - 0.5) * 0.45)) * (1.0 - smoothstep(0.8, 1.0, r));
	float coverage = edge * (0.55 + 0.45 * Fbm(uv * 7.0 + offset * 1.3));
	float ridge = 1.0 - abs(ValueNoise(uv * 4.5 + offset) * 2.0 - 1.0);
	float cracks = smoothstep(0.88, 0.98, ridge) * (1.0 - smoothstep(0.25, 0.85, r));
	vec2 cell = floor(uv * 22.0 + 100.0);
	float ember = step(0.97, HashUnit(uint(cell.x) * 7919u + uint(cell.y) * 104729u + seed)) * edge;
	float core = edge * (1.0 - smoothstep(0.0, 0.55, r));
	return vec2(coverage, heat * (cracks * 1.1 + ember * 0.9 + core * 0.25 * heat));
}

float CrackPath(float r, uint key, float amplitude)
{
	float x = r * 6.0;
	float i = floor(x);
	float a = (HashUnit(key * 7u + uint(i)) - 0.5) * amplitude;
	float b = (HashUnit(key * 7u + uint(i) + 1u) - 0.5) * amplitude;
	return mix(a, b, x - i);
}

float CrustHash(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

vec2 CrustHash2(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.103, 0.0973));
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.xx + p3.yz) * p3.zy);
}

vec2 CrustCells(vec2 p)
{
	vec2 cell = floor(p);
	vec2 f = fract(p);
	vec2 nearest = vec2(0.0);
	vec2 nearestCell = vec2(0.0);
	float best = 8.0;
	for (int j = -1; j <= 1; j++)
		for (int i = -1; i <= 1; i++)
		{
			vec2 g = vec2(float(i), float(j));
			vec2 d = g + 0.1 + 0.8 * CrustHash2(cell + g) - f;
			float dd = dot(d, d);
			if (dd < best)
			{
				best = dd;
				nearest = d;
				nearestCell = g;
			}
		}
	float border = 8.0;
	for (int j = -1; j <= 1; j++)
		for (int i = -1; i <= 1; i++)
		{
			vec2 g = nearestCell + vec2(float(i), float(j));
			vec2 d = g + 0.1 + 0.8 * CrustHash2(cell + g) - f;
			vec2 between = d - nearest;
			if (dot(between, between) > 1e-5)
				border = min(border, dot(0.5 * (nearest + d), normalize(between)));
		}
	return vec2(border, CrustHash(cell + nearestCell));
}

vec2 FractureShape(vec2 p, float pixel, float heat, float time)
{
	const float scale = 2.6;
	vec2 cells = CrustCells(p * scale);
	float spread = smoothstep(cells.y, cells.y + 0.25, heat * 1.25);
	float crack = Line(cells.x / scale, mix(0.004, 0.022, heat) * spread, pixel * 0.5) * spread;
	float flicker = 0.85 + 0.15 * sin(time * 11.0 + cells.y * 40.0);
	return vec2(crack, crack * heat * flicker);
}

struct Rime
{
	float Coverage;
	float Glow;
	float Shade;
	float Roughness;
};

Rime RimeSurface(vec2 p, vec2 vein, float inside, float fringe, float pixel, float glow, uint seed, float time)
{
	Rime rime = Rime(0.0, 0.0, 1.0, 0.7);
	if (inside < -fringe)
		return rime;
	vec2 offset = vec2(float(seed & 255u), float((seed >> 8) & 255u)) * 0.613;
	float e = inside / fringe;
	float detail = 1.0 - smoothstep(0.012, 0.04, pixel);
	float n = Fbm(p * 1.9 + offset);
	float powder = Fbm(p * 7.5 + offset * 1.7) * 0.75 + ValueNoise(p * 23.0 - offset) * 0.25 * detail + 0.125 * (1.0 - detail);
	float level = clamp(0.62 - 0.55 * e + (n - 0.5) * 0.5, 0.0, 1.2);
	float soft = max(fwidth(powder), 0.015);
	float dust = smoothstep(level - soft, level + soft, powder);
	vec2 warp = vec2(ValueNoise(p * 2.3 + offset), ValueNoise(p * 2.3 + offset + 7.3)) - 0.5;
	vec2 q = vein + offset + warp * 2.2;
	float ridge = 1.0 - abs(ValueNoise(q) * 2.0 - 1.0);
	float twig = 1.0 - abs(ValueNoise(q * 2.7 + 11.0) * 2.0 - 1.0);
	float feather = max(ridge, twig * 0.985);
	float reach = smoothstep(0.35, -0.9, e) * smoothstep(-1.0, -0.75, e);
	float spread = smoothstep(0.45, 0.7, ValueNoise(p * 1.1 + offset + 3.1));
	float veinSoft = max(fwidth(feather), 0.012);
	float fern = smoothstep(0.965 - veinSoft, 0.965 + veinSoft, feather) * smoothstep(-1.0, -0.2, e) * (1.0 - smoothstep(0.2, 0.6, e)) * (0.35 + 0.65 * spread) * (1.0 - 0.6 * reach * (1.0 - spread));
	float frost = max(dust, fern * 0.8);
	if (frost <= 0.0)
		return rime;
	float pool = ValueNoise(p * 4.3 + offset + 5.7);
	float glaze = smoothstep(0.72, 0.8, pool) * smoothstep(0.35, 0.8, e);
	float rim = (1.0 - smoothstep(0.0, 0.04, abs(pool - 0.72))) * smoothstep(0.35, 0.8, e);
	float clearIce = glaze;
	vec2 cell = floor(p * 13.0 + 300.0);
	uint key = uint(cell.x) * 7919u + uint(cell.y) * 104729u + seed;
	vec2 spot = (cell + 0.2 + 0.6 * vec2(HashUnit(key + 1u), HashUnit(key + 2u)) - 300.0) / 13.0;
	float glintSize = max(0.01, pixel * 0.8);
	float glint = step(0.85, HashUnit(key)) * exp(-dot(p - spot, p - spot) / (glintSize * glintSize)) * pow(0.5 + 0.5 * sin(time * 3.0 + HashUnit(key + 3u) * 40.0), 6.0) * dust * (1.0 - clearIce);
	float thick = clamp(0.35 + 0.65 * e, 0.0, 1.0) * (0.75 + 0.5 * (n - 0.5));
	float grain = mix(0.5, ValueNoise(p * 31.0 + offset * 2.3), detail);
	rime.Coverage = mix(frost * mix(0.5, 0.96, thick), 0.72, clearIce);
	rime.Shade = mix(0.86 + 0.2 * thick + 0.25 * (grain - 0.5) + 0.1 * rim, 0.8, clearIce);
	rime.Roughness = mix(0.75 - 0.2 * thick, 0.05, clearIce);
	rime.Glow = glow * (0.05 * dust + 0.35 * fern * (1.0 - dust) + 0.1 * rim) + glint * (0.5 + 0.6 * glow);
	return rime;
}

Rime FrostShape(vec2 uv, float radius, float pixel, float glow, uint seed, float time)
{
	vec2 p = uv * radius;
	float r = length(p);
	vec2 offset = vec2(float((seed >> 4) & 255u), float((seed >> 12) & 255u)) * 0.37;
	float edge = radius * 0.5 * (0.75 + 0.45 * Fbm(p / max(r, 1e-4) * 1.8 + offset));
	return RimeSurface(p, p * 4.0, edge - r, radius * 0.4, pixel, glow, seed, time);
}

Rime FrostLaneShape(vec2 uv, vec2 extent, float pixel, float glow, uint seed, float time)
{
	vec2 p = uv * extent;
	float run = max(extent.x - extent.y, 0.0);
	vec2 anchored = vec2(p.x + run, p.y);
	vec2 offset = vec2(float((seed >> 4) & 255u), float((seed >> 12) & 255u)) * 0.37;
	float tail = smoothstep(0.0, 0.9, anchored.x);
	float radius = extent.y * 0.5 * (0.72 + 0.4 * Fbm(vec2(anchored.x * 1.2, p.y * 2.0) + offset)) * mix(0.4, 1.0, tail);
	float d = SegmentDistance(p, vec2(-run, 0.0), vec2(run, 0.0));
	float fresh = clamp(anchored.x / max(2.0 * run, 1e-3), 0.0, 1.0);
	return RimeSurface(anchored, vec2(anchored.x * 5.0 + step(0.0, p.y) * 37.0, d * 2.2), radius - d, extent.y * 0.42, pixel, glow * mix(0.3, 1.0, fresh * fresh), seed, time);
}

vec2 CracksShape(vec2 uv, float pixel, float radius, float heat, uint seed)
{
	float r = length(uv);
	float turn = fract(atan(uv.y, uv.x) / Tau + 0.5);
	float count = 9.0 + float(seed % 4u);
	float sector = floor(turn * count);
	uint key = uint(sector) + seed * 31u;
	float local = turn * count - sector;
	float path = 0.5 + (HashUnit(key) - 0.5) * 0.35 + CrackPath(r, key, 0.5);
	float reach = 0.5 + 0.45 * HashUnit(key * 13u + 5u);
	float width = mix(0.045, 0.012, clamp(r / reach, 0.0, 1.0)) / radius;
	float crack = Line(abs(local - path) / count * Tau * r, width, pixel) * (1.0 - smoothstep(reach - 0.12, reach, r)) * smoothstep(0.06, 0.14, r);
	float fork = 0.25 + 0.3 * HashUnit(key * 17u + 3u);
	float side = HashUnit(key * 19u + 1u) > 0.5 ? 1.0 : -1.0;
	float branchPath = path + side * max(r - fork, 0.0) * 0.9 + CrackPath(r + 0.37, key + 101u, 0.25);
	float branchEnd = fork + 0.12 + 0.2 * HashUnit(key * 23u + 9u);
	float branch = Line(abs(local - branchPath) / count * Tau * r, width * 0.7, pixel) * step(fork, r) * (1.0 - smoothstep(branchEnd - 0.06, branchEnd, r));
	float lines = max(crack, branch);
	return vec2(max(lines, (1.0 - smoothstep(0.0, 0.3, r)) * 0.55), lines * heat * 0.7);
}

vec2 FissureShape(vec2 uv, vec2 extent, float pixel, float glow, float reach, float torn, uint seed, float time)
{
	vec2 p = uv * extent;
	float span = 2.0 * extent.x;
	float along = p.x + extent.x;
	vec2 offset = vec2(float(seed & 255u), float((seed >> 8) & 255u)) * 0.613;
	float front = reach * span;
	float opened = 1.0 - smoothstep(front - 0.3, front, along);
	float wander = (ValueNoise(vec2(along * 2.3, 5.0) + offset) - 0.5) * 0.24 + (ValueNoise(vec2(along * 7.5, 9.0) + offset) - 0.5) * 0.07;
	float q = p.y - wander;
	float taper = smoothstep(0.0, 0.3, along) * (1.0 - 0.6 * smoothstep(span - 0.6, span, along));
	float width = mix(0.024, 0.04, torn) * taper + 0.006;
	float crack = Line(abs(q), max(width, pixel * 0.6), pixel);
	float core = Line(abs(q), max(width * 0.4, pixel * 0.5), pixel);
	float cell = floor(along / 0.5);
	float local = along - cell * 0.5;
	uint key = uint(cell + 64.0) * 977u + seed * 31u;
	float side = HashUnit(key) > 0.5 ? 1.0 : -1.0;
	float start = 0.08 + 0.2 * HashUnit(key + 7u);
	float run = local - start;
	float reachOut = 0.12 + 0.24 * HashUnit(key + 3u);
	float branch = Line(abs(q - side * run * (0.7 + 0.7 * HashUnit(key + 11u))), max(width * 0.55, pixel * 0.5), pixel) * step(0.0, run) * (1.0 - smoothstep(reachOut * 0.6, reachOut, run));
	vec2 tip = vec2(span - 0.06, 0.0);
	vec2 at = vec2(along, q);
	float chevron = Line(min(SegmentDistance(at, tip, vec2(span - 0.5, 0.3)), SegmentDistance(at, tip, vec2(span - 0.5, -0.3))), max(width * 1.2, pixel * 0.7), pixel) * smoothstep(span - 0.5, span - 0.1, front) * (1.0 - torn);
	float n = Fbm(p * 2.4 + offset);
	float halfWidth = extent.y * (0.5 + 0.4 * n) * mix(0.75, 1.0, torn);
	float band = (1.0 - smoothstep(halfWidth * 0.45, halfWidth, abs(q))) * taper;
	vec2 grit = floor(p * 11.0 + 300.0);
	float pebble = step(0.93, HashUnit(uint(grit.x) * 7919u + uint(grit.y) * 104729u + seed)) * band;
	float lines = max(crack, max(branch * 0.85, chevron)) * opened;
	float shimmer = 0.75 + 0.25 * sin(time * 8.0 - along * 2.5);
	float coverage = max(lines, (band * mix(0.22, 0.55, torn) + pebble * 0.35) * opened);
	return vec2(coverage, glow * opened * (max(core, max(branch * 0.5, chevron)) * shimmer + band * 0.04));
}

float LightningBolt(vec2 uv, uint seed, float segments, float amplitude, float pixel)
{
	float x = clamp(uv.x, 0.0, 1.0) * segments;
	float i = floor(x);
	float a = (HashUnit(seed + uint(i) * 7919u) - 0.5) * amplitude * sin(3.14159265 * i / segments);
	float b = (HashUnit(seed + uint(i + 1.0) * 7919u) - 0.5) * amplitude * sin(3.14159265 * (i + 1.0) / segments);
	float d = abs(uv.y - mix(a, b, x - i));
	float taper = smoothstep(-0.02, 0.08, uv.x) * (1.0 - smoothstep(0.7, 1.0, uv.x));
	return (Line(d, max(0.018, pixel * 0.6), pixel) + 0.3 * exp(-d * d * 120.0)) * taper;
}

vec2 ChargeShape(vec2 uv, float pixel, float progress, uint seed, float time)
{
	float r = length(uv);
	float angle = atan(uv.y, uv.x);
	float window = 1.0 - smoothstep(0.92, 1.0, r);
	float wobble = (ValueNoise(vec2(angle * 6.0 + 40.0, time * 9.0 + float(seed & 63u))) - 0.5) * 0.06;
	float edge = r - 0.86 - wobble;
	float rim = Line(abs(edge), max(0.014, pixel * 0.7), pixel) + 0.35 * exp(-edge * edge * 196.0);
	float flicker = 0.7 + 0.3 * sin(time * 61.0 + float(seed & 255u));
	uint beat = uint(time * 14.0);
	float arcs = 0.0;
	for (uint k = 0u; k < 4u; k++)
	{
		uint h = Hash(seed * 2654435761u + beat * 40503u + k * 977u);
		if (HashUnit(h ^ 0x3333u) > 0.35 + 0.6 * progress)
			continue;
		float a = HashUnit(h) * Tau;
		vec2 start = vec2(cos(a), sin(a)) * 0.86;
		float b = a + (HashUnit(h ^ 0x9E37u) - 0.5) * 2.2;
		vec2 end = vec2(cos(b), sin(b)) * mix(0.1, 0.55, HashUnit(h ^ 0x51F1u)) * (1.2 - progress);
		vec2 axis = end - start;
		float span = max(length(axis), 1e-3);
		vec2 along = axis / span;
		vec2 local = vec2(dot(uv - start, along), dot(uv - start, vec2(-along.y, along.x))) / span;
		arcs += LightningBolt(local, h, 5.0, 0.35, pixel / span);
	}
	float core = exp(-r * r * 6.0) * progress * progress;
	float fill = (1.0 - smoothstep(0.75, 0.88, r)) * (0.05 + 0.1 * progress);
	float glow = (rim * (0.5 + 0.9 * progress) * flicker + arcs * (0.6 + 0.8 * progress) + core * 0.9 + fill) * window;
	return vec2((1.0 - smoothstep(0.7, 0.9, r)) * 0.3 * progress, glow);
}

float LightningShape(vec2 uv, uint seed, float pixel)
{
	float bolt = LightningBolt(uv, seed, 6.0, 0.5, pixel);
	vec2 branch = Rotate(uv - vec2(0.45, 0.0), 0.7) * 1.6;
	return bolt + 0.7 * LightningBolt(branch, seed * 31u + 7u, 4.0, 0.4, pixel);
}

float OrbShape(vec2 uv, uint seed, float time)
{
	float r2 = dot(uv, uv);
	vec2 offset = vec2(float(seed & 63u), float((seed >> 6) & 63u)) * 0.37;
	float swirl = ValueNoise(Rotate(uv, time * 2.5) * 3.0 + offset);
	float body = exp(-4.0 * r2) * (0.7 + 0.6 * swirl);
	float core = exp(-24.0 * r2);
	return (body + 1.5 * core) * (1.0 - smoothstep(0.8, 1.0, sqrt(r2)));
}

float SlashShape(vec2 uv, vec4 params, float pixel)
{
	float r = length(uv);
	float angle = atan(uv.y, uv.x) * (params.w < 0.0 ? -1.0 : 1.0);
	float halfArc = params.x;
	float head = mix(-halfArc, halfArc, params.y);
	float behind = head - angle;
	float trail = exp(-max(behind, 0.0) / max(abs(params.w), 0.05)) * smoothstep(-0.12, 0.0, behind) * step(-halfArc - 0.1, angle);
	float band = smoothstep(params.z, params.z + 0.2, r) * (1.0 - smoothstep(0.95, 1.0, r));
	float blade = (0.9 * Line(abs(r - 0.9), 0.025, pixel) + 0.35 * exp(-pow((r - 0.88) / 0.07, 2.0))) * (1.0 - smoothstep(0.95, 1.0, r));
	return (band * 0.2 + blade) * trail;
}

float SnowflakeShape(vec2 uv, float pixel)
{
	float r = length(uv);
	float sector = Tau / 6.0;
	float a = mod(atan(uv.y, uv.x) + sector * 0.5, sector) - sector * 0.5;
	vec2 p = vec2(cos(a), abs(sin(a))) * r;
	float arms = SegmentDistance(p, vec2(0.0), vec2(0.88, 0.0));
	arms = min(arms, SegmentDistance(p, vec2(0.45, 0.0), vec2(0.66, 0.2)));
	arms = min(arms, SegmentDistance(p, vec2(0.68, 0.0), vec2(0.82, 0.12)));
	return Line(arms, 0.085, pixel) + 0.5 * exp(-r * r * 30.0);
}

float BeamShape(vec2 uv)
{
	float across = exp(-uv.y * uv.y * 9.0) * 0.6 + exp(-uv.y * uv.y * 70.0);
	float along = smoothstep(-1.0, -0.85, uv.x) * (1.0 - smoothstep(-0.2, 1.0, uv.x));
	return across * along;
}

float InfernoShape(vec2 uv, uint seed, vec4 params, float time)
{
	float r = length(uv);
	vec2 offset = vec2(float(seed & 63u), float((seed >> 6) & 63u)) * 0.53;
	vec2 outward = uv / max(r, 1e-4);
	float body = clamp(params.x, 0.05, 0.95);
	vec2 spun = Rotate(uv, time * params.z);
	float flow = time * params.y;
	float tongues = Fbm(spun * 2.0 - outward * flow + offset);
	float lick = ValueNoise(spun * 4.2 - outward * (flow * 1.4) + offset * 1.7 + 9.2);
	float rise = max(uv.y, 0.0) / max(r, 1e-4);
	float reach = body + (1.0 - body) * params.w * (1.2 * tongues - 0.2 + 0.12 * lick + 0.35 * rise * tongues);
	float shell = 1.0 - smoothstep(reach - 0.18 * (1.0 - body) - 0.05, reach, r);
	float churn = Fbm(spun * 3.4 + vec2(0.0, flow * 0.7) + offset * 2.3);
	float core = exp(-3.0 * r * r / (body * body));
	float density = shell * (0.4 + 0.6 * churn) * (0.55 + 0.45 * smoothstep(0.0, body, r)) + core * (0.6 + 0.4 * churn);
	return density * (1.0 - smoothstep(0.88, 1.0, r));
}

float HexNorm(vec2 p)
{
	p = abs(p);
	return max(p.y, p.x * 0.8660254 + p.y * 0.5) / 0.8660254;
}

float HexShape(vec2 uv, uint seed, vec4 params, float pixel)
{
	if (params == vec4(0.0))
		params = vec4(0.85, 0.8, 0.0, 0.0);
	float h = HexNorm(uv);
	float edge = (1.0 - h) * 0.8660254;
	float inside = clamp(edge / max(pixel, 1e-4) + 0.5, 0.0, 1.0);
	if (inside <= 0.0)
		return 0.0;
	float rim = Line(edge, max(0.045, pixel * 0.75), pixel) + 0.45 * exp(-edge * 9.0);
	float bevel = 0.3 * Line(abs(edge - 0.15), max(0.012, pixel * 0.5), pixel);
	float glow = params.x * (0.18 + 0.82 * h * h * h) + params.y * (rim + bevel);
	if (params.z > 0.0)
	{
		vec2 q = uv - (vec2(HashUnit(seed * 3u + 1u), HashUnit(seed * 3u + 2u)) - 0.5) * 0.6;
		float r = length(q);
		float angle = atan(q.y, q.x);
		float count = 4.0 + float(seed % 3u);
		float turn = fract(angle / Tau + HashUnit(seed * 5u + 3u)) * count;
		float sector = floor(turn);
		uint key = uint(sector) + seed * 31u;
		float path = 0.5 + (HashUnit(key) - 0.5) * 0.45 + CrackPath(r, key, 0.55);
		float reach = params.z * (1.3 + 0.65 * HashUnit(key * 13u + 5u));
		float width = max(mix(0.035, 0.012, clamp(r / 1.2, 0.0, 1.0)), pixel * 0.6);
		float crack = Line(abs(turn - sector - path) / count * Tau * r, width, pixel) * (1.0 - smoothstep(reach - 0.15, reach, r));
		float ring = Line(abs(r - 0.3 - 0.06 * sin(angle * 3.0 + float(seed & 7u))), max(0.012, pixel * 0.5), pixel) * smoothstep(0.35, 0.7, params.z);
		glow *= 1.0 - 0.3 * params.z;
		glow += (crack + 0.7 * ring) * (0.9 + 0.6 * params.z) + 0.6 * params.z * exp(-r * r * 60.0);
	}
	glow += params.w * (0.55 + 0.45 * (1.0 - h) + 0.5 * rim);
	return glow * inside;
}

float ParticleShape(uint shape, vec2 uv, uint seed, vec4 params, float pixel, float time)
{
	float r2 = dot(uv, uv);
	float edge = 1.0 - smoothstep(0.75, 1.0, sqrt(r2));
	if (shape == ShapeSpark)
		return exp(-3.0 * uv.x * uv.x - 9.0 * uv.y * uv.y) * edge;
	if (shape == ShapeSmoke)
	{
		vec2 offset = vec2(float(seed & 255u), float((seed >> 8) & 255u)) * 0.37;
		float noise = ValueNoise(uv * 2.2 + offset) * 0.65 + ValueNoise(uv * 4.7 + offset * 1.7) * 0.35;
		return pow(clamp(1.0 - r2, 0.0, 1.0), 1.5) * smoothstep(0.15, 0.75, noise);
	}
	if (shape == ShapeRing)
	{
		if (params.x > 0.0)
			return RingGlow(sqrt(r2), params.y, max(params.x, pixel * 0.6), pixel);
		float distance = (sqrt(r2) - 0.78) / 0.12;
		return exp(-distance * distance) * edge;
	}
	if (shape == ShapeStar)
	{
		float core = exp(-9.0 * r2);
		float flare = exp(-40.0 * uv.x * uv.x - 2.5 * uv.y * uv.y) + exp(-40.0 * uv.y * uv.y - 2.5 * uv.x * uv.x);
		return min((core + 0.55 * flare) * edge, 1.0);
	}
	if (shape == ShapeShard)
	{
		vec2 p = params.x > 0.5 ? uv.yx : uv;
		return 1.0 - smoothstep(0.8, 1.0, abs(p.x) * 2.2 + abs(p.y));
	}
	if (shape == ShapeDebris)
		return 1.0 - smoothstep(0.65, 0.85, max(abs(uv.x), abs(uv.y)));
	if (shape == ShapeFlame)
	{
		vec2 p = vec2(uv.x * (1.3 + 0.9 * (uv.y + 1.0)), uv.y * 0.85 + 0.15);
		return exp(-3.2 * dot(p, p)) * edge;
	}
	if (shape == ShapeOrb)
		return OrbShape(uv, seed, time);
	if (shape == ShapeLightning)
		return LightningShape(uv, seed, pixel);
	if (shape == ShapeRune)
		return RuneCircle(uv, pixel, max(params.w, 0.05), params.x, params.y, 0.0, params.z, seed, time);
	if (shape == ShapeSlash)
		return SlashShape(uv, params, pixel);
	if (shape == ShapeSnowflake)
		return SnowflakeShape(uv, pixel);
	if (shape == ShapeBeam)
		return BeamShape(uv);
	if (shape == ShapeInferno)
		return InfernoShape(uv, seed, params, time);
	if (shape == ShapeHex)
		return HexShape(uv, seed, params, pixel);
	return exp(-4.0 * r2) * edge;
}
