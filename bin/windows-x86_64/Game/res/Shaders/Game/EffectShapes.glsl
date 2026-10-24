const uint ShapeOrb = 8u;
const uint ShapeLightning = 9u;
const uint ShapeRune = 10u;
const uint ShapeSlash = 11u;
const uint ShapeSnowflake = 12u;
const uint ShapeBeam = 13u;

const uint DecalRunes = 0u;
const uint DecalSector = 1u;
const uint DecalLane = 2u;
const uint DecalRing = 3u;
const uint DecalGlow = 4u;
const uint DecalScorch = 5u;
const uint DecalFrost = 6u;
const uint DecalCracks = 7u;
const uint DecalFracture = 8u;

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

vec2 FrostShape(vec2 uv, float strength, uint seed, float time)
{
	float r = length(uv);
	vec2 offset = vec2(float(seed & 255u), float((seed >> 8) & 255u)) * 0.613;
	float n = Fbm(uv * 3.0 + offset);
	float edge = (1.0 - smoothstep(0.55, 1.0, r + (n - 0.5) * 0.35)) * (1.0 - smoothstep(0.85, 1.0, r));
	float angle = atan(uv.y, uv.x);
	float spikes = pow(abs(sin(angle * 9.0 + n * 3.0)), 14.0) * smoothstep(0.2, 0.9, r) * edge;
	vec2 cell = floor(uv * 14.0 + 200.0);
	float sparkleSeed = HashUnit(uint(cell.x) * 7919u + uint(cell.y) * 104729u + seed);
	float sparkle = step(0.9, sparkleSeed) * pow(0.5 + 0.5 * sin(time * 5.0 + sparkleSeed * 40.0), 8.0) * edge;
	return vec2(edge * (0.35 + 0.35 * n) * strength, strength * (spikes * 0.7 + sparkle * 1.5 + edge * 0.12));
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
	return exp(-4.0 * r2) * edge;
}
