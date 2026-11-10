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
const uint DecalCaustic = 12u;
const uint DecalNova = 13u;
const uint DecalVoid = 14u;
const uint DecalCometMark = 15u;
const uint DecalStormMark = 16u;
const uint DecalVoidMark = 17u;
const uint DecalFirebombMark = 18u;
const uint DecalContact = 19u;

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

float LightningBolt(vec2 uv, uint seed, float segments, float amplitude, float pixel);

const uint RuneCounts[16] = uint[16](3u, 2u, 2u, 2u, 2u, 1u, 3u, 3u, 3u, 2u, 3u, 3u, 3u, 2u, 3u, 3u);

const vec3 RuneDots[16] = vec3[16](
	vec3(0.0), vec3(0.0), vec3(0.0, 0.0, 0.13), vec3(0.0), vec3(0.0), vec3(0.12, 0.0, 0.14), vec3(0.0), vec3(0.0, 0.35, 0.11),
	vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.28, -0.45, 0.11), vec3(0.0), vec3(0.0));

const vec2 RuneStrokes[144] = vec2[144](
	vec2(0.0, -0.9), vec2(0.0, 0.0), vec2(0.0, 0.9),
	vec2(0.0, 0.05), vec2(0.12, 0.45), vec2(0.45, 0.8),
	vec2(0.0, 0.05), vec2(-0.12, 0.45), vec2(-0.45, 0.8),

	vec2(-0.1, -0.9), vec2(-0.1, 0.0), vec2(-0.1, 0.9),
	vec2(-0.1, 0.6), vec2(0.65, 0.35), vec2(-0.1, -0.05),
	vec2(0.0), vec2(0.0), vec2(0.0),

	vec2(0.0, 0.88), vec2(0.7, 0.0), vec2(0.0, -0.88),
	vec2(0.0, 0.88), vec2(-0.7, 0.0), vec2(0.0, -0.88),
	vec2(0.0), vec2(0.0), vec2(0.0),

	vec2(-0.45, 0.85), vec2(0.15, 0.05), vec2(0.45, -0.85),
	vec2(0.45, 0.85), vec2(-0.15, 0.05), vec2(-0.45, -0.85),
	vec2(0.0), vec2(0.0), vec2(0.0),

	vec2(0.0, -0.9), vec2(0.0, -0.3), vec2(0.0, 0.3),
	vec2(-0.5, 0.88), vec2(0.0, 0.05), vec2(0.5, 0.88),
	vec2(0.0), vec2(0.0), vec2(0.0),

	vec2(0.35, 0.85), vec2(-0.75, 0.0), vec2(0.35, -0.85),
	vec2(0.0), vec2(0.0), vec2(0.0),
	vec2(0.0), vec2(0.0), vec2(0.0),

	vec2(0.0, -0.9), vec2(0.0, 0.0), vec2(0.0, 0.88),
	vec2(-0.45, 0.35), vec2(-0.1, 0.55), vec2(0.0, 0.88),
	vec2(0.45, 0.35), vec2(0.1, 0.55), vec2(0.0, 0.88),

	vec2(-0.45, 0.35), vec2(0.0, 1.0), vec2(0.45, 0.35),
	vec2(-0.45, 0.35), vec2(0.0, -0.3), vec2(0.45, 0.35),
	vec2(0.0, 0.0), vec2(0.0, -0.45), vec2(0.0, -0.9),

	vec2(0.0, -0.9), vec2(0.0, 0.0), vec2(0.0, 0.9),
	vec2(0.0, 0.5), vec2(0.5, 0.0), vec2(0.0, -0.5),
	vec2(0.0, 0.5), vec2(-0.5, 0.0), vec2(0.0, -0.5),

	vec2(-0.05, 0.9), vec2(-0.55, 0.45), vec2(-0.05, 0.05),
	vec2(0.05, -0.05), vec2(0.55, -0.45), vec2(0.05, -0.9),
	vec2(0.0), vec2(0.0), vec2(0.0),

	vec2(0.0, -0.9), vec2(0.0, -0.45), vec2(0.0, 0.3),
	vec2(-0.5, 0.88), vec2(-0.45, 0.2), vec2(0.0, 0.05),
	vec2(0.5, 0.88), vec2(0.45, 0.2), vec2(0.0, 0.05),

	vec2(0.0, -0.9), vec2(0.0, 0.0), vec2(0.0, 0.9),
	vec2(-0.45, 0.6), vec2(0.0, 0.5), vec2(0.45, 0.3),
	vec2(-0.45, -0.05), vec2(0.0, -0.15), vec2(0.45, -0.35),

	vec2(0.0, -0.9), vec2(0.15, 0.0), vec2(0.0, 0.9),
	vec2(0.05, 0.6), vec2(0.3, 0.5), vec2(0.5, 0.78),
	vec2(0.08, -0.05), vec2(-0.25, -0.15), vec2(-0.5, 0.08),

	vec2(0.0, -0.88), vec2(-0.6, 0.15), vec2(0.05, 0.85),
	vec2(0.05, 0.85), vec2(0.55, 0.4), vec2(0.1, 0.1),
	vec2(0.0), vec2(0.0), vec2(0.0),

	vec2(-0.42, -0.65), vec2(-0.55, 0.15), vec2(-0.25, 0.7),
	vec2(0.0, -0.88), vec2(0.08, 0.0), vec2(0.0, 0.9),
	vec2(0.42, -0.65), vec2(0.55, 0.15), vec2(0.25, 0.7),

	vec2(0.0, -0.9), vec2(0.0, 0.0), vec2(0.0, 0.9),
	vec2(-0.5, 0.25), vec2(0.0, 0.55), vec2(0.5, 0.25),
	vec2(0.0, -0.9), vec2(0.35, -0.85), vec2(0.48, -0.55));

vec4 BrushStroke(vec2 p, vec2 a, vec2 b, vec2 c, float width)
{
	vec4 best = vec4(1e3, 0.0, 0.0, width);
	vec2 prev = a;
	for (int i = 1; i <= 6; i++)
	{
		float s = float(i) / 6.0;
		vec2 next = mix(mix(a, b, s), mix(b, c, s), s);
		vec2 pa = p - prev;
		vec2 ba = next - prev;
		float len = max(length(ba), 1e-5);
		float h = clamp(dot(pa, ba) / (len * len), 0.0, 1.0);
		float along = (float(i - 1) + h) / 6.0;
		float w = width * (0.6 + 0.55 * smoothstep(0.0, 0.1, along) - 0.15 * smoothstep(0.1, 0.3, along)) * (1.0 - 0.78 * smoothstep(0.35, 1.0, along));
		float sd = length(pa - ba * h) - w;
		if (sd < best.x)
			best = vec4(sd, along, (ba.x * pa.y - ba.y * pa.x) / len, w);
		prev = next;
	}
	return best;
}

vec4 RuneGlyph(vec2 g, uint key, float pixel)
{
	uint h = Hash(key * 2654435761u + 17u);
	uint index = h % 16u;
	g.x = (h & 0x10000u) != 0u ? -g.x : g.x;
	g = Rotate(g, (HashUnit(h ^ 0x5BD1u) - 0.5) * 0.2);
	g /= mix(0.92, 1.05, HashUnit(h ^ 0x1F3Du));
	float width = 0.155 * mix(0.85, 1.15, HashUnit(h ^ 0x7A11u));
	vec4 best = vec4(1e3, 0.0, 0.0, width);
	float stroke = 0.0;
	for (uint s = 0u; s < 3u; s++)
	{
		if (s >= RuneCounts[index])
			break;
		uint base = index * 9u + s * 3u;
		vec4 candidate = BrushStroke(g, RuneStrokes[base], RuneStrokes[base + 1u], RuneStrokes[base + 2u], width * (s == 0u ? 1.1 : 0.92));
		if (candidate.x < best.x)
		{
			best = candidate;
			stroke = float(s);
		}
	}
	vec3 blot = RuneDots[index];
	if (blot.z > 0.0)
	{
		float sd = length(g - blot.xy) - blot.z;
		if (sd < best.x)
		{
			best = vec4(sd, 0.3, 0.0, blot.z);
			stroke = 3.0;
		}
	}
	float salt = float(h & 255u) * 0.37;
	float across = best.z / max(best.w, 1e-3);
	float ragged = ValueNoise(vec2(best.y * 11.0 + stroke * 7.3 + salt, sign(across) * 2.5 + 5.0));
	float sd = best.x + (ragged - 0.5) * 0.5 * best.w;
	float fibre = ValueNoise(vec2(across * 3.6 + stroke * 5.1 + salt, best.y * 2.4));
	float dry = smoothstep(0.2, 0.6, fibre + 0.32 - 0.45 * smoothstep(0.5, 1.0, best.y));
	float ink = 1.0 - smoothstep(-pixel, pixel, sd);
	float depth = clamp(-sd / max(best.w, 1e-3), 0.0, 1.0);
	float paint = ink * mix(0.2, 0.6, dry);
	float core = smoothstep(0.25, 0.85, depth + 0.25 * (dry - 0.5)) * mix(0.45, 1.0, dry) * ink;
	float outside = max(sd, 0.0);
	float halo = exp(-outside * outside / 0.05);
	return vec4(ink, paint, core, halo);
}

struct RuneInk
{
	float Ink;
	float Glow;
	float Hot;
	float Cell;
};

RuneInk RuneRing(float r, float angle, float inner, float outer, float spin, uint seed, float pixel, float time)
{
	RuneInk runes = RuneInk(0.0, 0.0, 0.0, 0.0);
	float band = outer - inner;
	float mid = 0.5 * (inner + outer);
	if (abs(r - mid) > band)
		return runes;
	float cells = max(6.0, floor(Tau * mid / (band * 1.05)));
	float spun = fract((angle + spin) / Tau) * cells;
	float cellWidth = Tau * mid / cells;
	float scale = band * 0.44;
	runes.Cell = floor(spun);
	uint key = uint(runes.Cell) + seed * 131u;
	float wave = pow(0.5 + 0.5 * cos(angle - time * 1.3 + float(seed & 31u)), 10.0);
	vec2 g = vec2((fract(spun) - 0.5) * cellWidth, r - mid) / scale;
	if (abs(g.y) < 1.4)
	{
		vec4 glyph = RuneGlyph(g, key, pixel / scale);
		float pulse = 0.8 + 0.2 * sin(time * 2.3 + HashUnit(key * 31u) * Tau);
		float lit = (0.75 + 0.9 * wave) * pulse;
		runes.Ink = glyph.x;
		runes.Glow = (glyph.y * 0.6 + glyph.z * 0.95 + glyph.w * 0.22) * lit;
		runes.Hot = glyph.z * 0.6 * lit * (0.35 + 0.65 * wave);
	}
	float gap = (0.5 - abs(fract(spun) - 0.5)) * cellWidth;
	float diamond = gap / (band * 0.06) + abs(r - mid) / (band * 0.15);
	float separator = 1.0 - smoothstep(1.0 - 2.0 * pixel / (band * 0.06), 1.0, diamond);
	float frameRadius = inner - band * 0.1;
	float frameWidth = band * (0.022 + 0.03 * ValueNoise(vec2(angle * mid * 6.0, 1.0 + float(seed & 15u))));
	float frame = 1.0 - smoothstep(frameWidth - pixel, frameWidth + pixel, abs(r - frameRadius));
	float trim = (separator * 0.7 + frame * 0.45) * (0.8 + 0.6 * wave);
	runes.Ink = max(runes.Ink, max(separator, frame) * 0.8);
	runes.Glow += trim;
	return runes;
}

vec3 MarkRunes(vec3 mark, RuneInk runes, float strength, float darken)
{
	return vec3(max(mark.x, runes.Ink * darken), mark.y + runes.Glow * strength, mark.z + runes.Hot * strength);
}

float MarkRim(float r, float radius, float width, float pixel)
{
	float d = r - radius;
	return Line(abs(d), max(width, pixel * 0.7), pixel) + 0.3 * exp(-d * d / (width * width * 14.0));
}

float MarkOutline(float r, float radius)
{
	float d = (r - radius - 0.07) / 0.06;
	return 0.55 * exp(-d * d);
}

vec3 MarkAccumulate(vec3 mark, float glow, float hot)
{
	return vec3(mark.x, mark.y + glow, mark.z + glow * hot);
}

vec3 CometMarkShape(vec2 p, float radius, float pixel, float progress, float appear, uint seed, float time)
{
	float grow = smoothstep(0.0, 1.0, appear);
	float r = length(p) / mix(0.88, 1.0, grow);
	float n = r / radius;
	if (n > 1.12 || grow <= 0.0)
		return vec3(0.0);
	float angle = atan(p.y, p.x);
	bool live = progress >= 0.0;
	float t = clamp(progress, 0.0, 1.0);
	vec2 offset = vec2(float(seed & 255u), float((seed >> 8) & 255u)) * 0.53;
	vec3 mark = vec3(MarkOutline(r, radius), 0.0, 0.0);

	float soft = live ? mix(0.6, 0.05, t) : 0.3;
	float shadow = (1.0 - smoothstep(1.0 - soft, 1.0, n)) * (live ? mix(0.15, 0.72, t * t) : 0.08);
	mark.x = max(mark.x, shadow);

	mark = MarkAccumulate(mark, MarkRim(r, radius, 0.034, pixel) * (1.0 + 0.3 * t), 0.15 * t);
	mark = MarkRunes(mark, RuneRing(r, angle, radius - 0.5, radius - 0.12, time * 0.12, seed, pixel, time), 0.55 + 0.4 * t, 0.75);

	if (live)
	{
		float front = 1.0 - t;
		float heated = smoothstep(front - 0.01, front + 0.02, n) * step(n, 1.0);
		float ember = 0.55 + 0.9 * Fbm(p * 1.4 + offset + vec2(time * 0.25, -time * 0.18));
		float ridge = 1.0 - abs(ValueNoise(p * 2.7 + offset * 1.7) * 2.0 - 1.0);
		float veins = smoothstep(0.955, 0.99, ridge) * heated * smoothstep(0.35, 0.85, t);
		mark = MarkAccumulate(mark, heated * (0.03 + 0.06 * t) * ember, 0.0);
		mark = MarkAccumulate(mark, veins * 0.45, 0.1);
		float gap = (n - front) * radius / 0.12;
		float edge = Line(abs(n - front) * radius, max(0.026, pixel * 0.7), pixel) + 0.25 * exp(-gap * gap);
		mark = MarkAccumulate(mark, edge * (0.75 + 0.35 * t) * smoothstep(0.0, 0.1, front), 0.25);
	}
	return vec3(mark.x, mark.y, mark.z / max(mark.y, 1e-4)) * vec3(grow, grow, 1.0);
}

vec3 StormMarkShape(vec2 p, float radius, float pixel, float progress, float appear, uint seed, float time)
{
	float grow = smoothstep(0.0, 1.0, appear);
	float r = length(p) / mix(0.8, 1.0, grow);
	float n = r / radius;
	if (n > 1.15 || grow <= 0.0)
		return vec3(0.0);
	float angle = atan(p.y, p.x);
	bool live = progress >= 0.0;
	float t = clamp(progress, 0.0, 1.0);
	float beat = floor(time * mix(14.0, 36.0, t));
	float flicker = 0.75 + 0.25 * HashUnit(uint(beat) * 7919u + seed);
	vec3 mark = vec3(MarkOutline(r, radius) + (1.0 - smoothstep(0.2, 1.0, n)) * 0.3 * t, 0.0, 0.0);

	float wobble = (ValueNoise(vec2(angle * 5.0 + 30.0, time * 20.0 + float(seed & 63u))) - 0.5) * (0.016 + 0.03 * t);
	mark = MarkAccumulate(mark, MarkRim(r + wobble, radius, 0.032, pixel) * flicker * (1.0 + 0.4 * t), 0.08 + 0.15 * t);

	RuneInk runes = RuneRing(r, angle, radius - 0.4, radius - 0.1, -time * 0.25, seed, pixel, time);
	float lit = HashUnit(uint(runes.Cell) * 977u + uint(beat) * 131u + seed) < (live ? 0.4 + 0.55 * t : 0.65) ? 1.0 : 0.3;
	mark = MarkRunes(mark, runes, 0.7 * lit, 0.6);

	if (live)
	{
		uint arcBeat = uint(time * 16.0);
		for (uint k = 0u; k < 4u; k++)
		{
			uint h = Hash(seed * 2654435761u + arcBeat * 40503u + k * 977u);
			if (HashUnit(h ^ 0x3333u) > 0.45 + 0.6 * t)
				continue;
			float a = (float(k) + HashUnit(h)) / 4.0 * Tau;
			vec2 start = vec2(cos(a), sin(a)) * (radius - 0.04);
			float b = a + (HashUnit(h ^ 0x9E37u) - 0.5) * 1.1;
			vec2 end = vec2(cos(b), sin(b)) * radius * mix(0.7, 0.05, t) * mix(0.8, 1.1, HashUnit(h ^ 0x51F1u));
			vec2 axis = end - start;
			float span = max(length(axis), 1e-3);
			vec2 along = axis / span;
			vec2 local = vec2(dot(p - start, along), dot(p - start, vec2(-along.y, along.x))) / span;
			mark = MarkAccumulate(mark, LightningBolt(local, h, 6.0, 0.3, pixel / span) * (0.9 + 0.6 * t), 0.6);
		}
	}
	return vec3(mark.x, mark.y, mark.z / max(mark.y, 1e-4)) * vec3(grow, grow, 1.0);
}

vec3 VoidMarkShape(vec2 p, float radius, float pixel, float progress, float appear, uint seed, float time)
{
	float grow = smoothstep(0.0, 1.0, appear);
	float r = length(p) / mix(0.9, 1.0, grow);
	float n = r / radius;
	if (n > 1.12 || grow <= 0.0)
		return vec3(0.0);
	float angle = atan(p.y, p.x);
	float t = clamp(progress, 0.0, 1.0);
	vec3 mark = vec3(MarkOutline(r, radius), 0.0, 0.0);
	mark = MarkAccumulate(mark, MarkRim(r, radius, 0.04, pixel) * (1.0 + 0.3 * t), 0.0);
	mark = MarkRunes(mark, RuneRing(r, angle, radius - 0.6, radius - 0.14, -(time * 0.2 + t * t * 1.2), seed, pixel, time), 0.5 + 0.35 * t, 0.8);
	return vec3(mark.x, mark.y, mark.z / max(mark.y, 1e-4)) * vec3(grow, grow, 1.0);
}

vec3 FirebombMarkShape(vec2 p, float radius, float pixel, float progress, float appear, uint seed, float time)
{
	float grow = smoothstep(0.0, 1.0, appear);
	float r = length(p) / mix(0.85, 1.0, grow);
	float n = r / radius;
	if (n > 1.12 || grow <= 0.0)
		return vec3(0.0);
	float angle = atan(p.y, p.x);
	bool live = progress >= 0.0;
	float t = clamp(progress, 0.0, 1.0);
	vec2 offset = vec2(float(seed & 255u), float((seed >> 8) & 255u)) * 0.61;
	vec2 dir = p / max(length(p), 1e-4);
	float phase = live ? 3.0 * t + 4.0 * t * t : time * 1.2;
	float tick = fract(phase);
	float beat = exp(-tick * 7.0) * (live ? 1.0 : 0.4);
	vec3 mark = vec3(MarkOutline(r, radius) + (1.0 - smoothstep(0.3, 1.0, n)) * (live ? 0.06 + 0.22 * t : 0.04), 0.0, 0.0);

	mark = MarkAccumulate(mark, MarkRim(r, radius, 0.034, pixel) * (0.85 + 0.65 * beat), 0.08 + 0.3 * beat * t);
	float inner = radius - 0.46;
	mark = MarkRunes(mark, RuneRing(r, angle, inner, radius - 0.12, time * 0.15, seed, pixel, time), 0.5 + 0.25 * t + 0.3 * beat, 0.7);

	if (live)
	{
		float edge = inner - 0.08;
		float depth = edge - r;
		float tongue = ValueNoise(dir * edge * 4.5 + offset + vec2(time * 2.3, -time * 1.1));
		float height = mix(0.08, 0.32, t) * (0.3 + 1.4 * tongue * tongue);
		float body = clamp(1.0 - depth / max(height, 1e-3), 0.0, 1.0) * step(0.0, depth);
		float erode = Fbm(p * 3.8 - dir * time * 2.2 + offset * 1.3);
		float flame = smoothstep(0.0, 0.25, body) * smoothstep(0.4, 0.62, erode * 0.75 + body * 0.5);
		mark = MarkAccumulate(mark, flame * (0.25 + 0.65 * body) * (0.55 + 0.5 * t), 0.1 * body);
	}
	return vec3(mark.x, mark.y, mark.z / max(mark.y, 1e-4)) * vec3(grow, grow, 1.0);
}

vec3 SpellMarkShape(uint shape, vec2 p, float radius, float pixel, float progress, float appear, uint seed, float time)
{
	if (shape == DecalCometMark)
		return CometMarkShape(p, radius, pixel, progress, appear, seed, time);
	if (shape == DecalStormMark)
		return StormMarkShape(p, radius, pixel, progress, appear, seed, time);
	if (shape == DecalFirebombMark)
		return FirebombMarkShape(p, radius, pixel, progress, appear, seed, time);
	return VoidMarkShape(p, radius, pixel, progress, appear, seed, time);
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

float NovaArm(vec2 p, float angle, float reach, float pixel, uint key)
{
	vec2 d = vec2(cos(angle), sin(angle));
	float x = dot(p, d);
	if (x < 0.0 || x > reach + 0.25)
		return 0.0;
	float y = dot(p, vec2(-d.y, d.x)) - CrackPath(x * 0.35, key, 0.2);
	if (abs(y) > 0.4)
		return 0.0;
	float width = mix(0.034, 0.011, clamp(x / max(reach, 1e-3), 0.0, 1.0));
	float vein = Line(abs(y), max(width, pixel * 0.5), pixel) * smoothstep(0.12, 0.4, x) * (1.0 - smoothstep(reach * 0.88, reach, x));
	vec2 q = vec2(x, abs(y));
	uint side = y > 0.0 ? 1u : 0u;
	float spacing = 0.26;
	float cell = floor(q.x / spacing);
	for (int i = 0; i < 2; i++)
	{
		float k = cell - float(i);
		if (k < 1.0)
			break;
		uint h = Hash(key * 131u + uint(k) * 7u + side);
		float start = (k + 0.35 * float(h & 255u) / 255.0) * spacing;
		float run = (0.12 + 0.22 * float((h >> 8) & 255u) / 255.0) * (1.0 - 0.6 * clamp(start / max(reach, 1e-3), 0.0, 1.0));
		float tilt = 0.85 + 0.25 * float((h >> 16) & 255u) / 255.0;
		vec2 tip = vec2(start, 0.0) + vec2(cos(tilt), sin(tilt)) * run;
		float twig = Line(SegmentDistance(q, vec2(start, 0.0), tip), max(width * 0.6, pixel * 0.45), pixel);
		vein = max(vein, twig * step(float(h >> 24) / 255.0, 0.82) * step(start, reach * 0.92));
	}
	return vein;
}

Rime NovaRime(vec2 p, float inside, float fringe, float pixel, float glow, uint seed, float time)
{
	Rime rime = Rime(0.0, 0.0, 1.0, 0.7);
	if (inside < -fringe)
		return rime;
	vec2 offset = vec2(float(seed & 255u), float((seed >> 8) & 255u)) * 0.613;
	float e = inside / fringe;
	float n = ValueNoise(p * 1.9 + offset);
	float powder = ValueNoise(p * 7.5 + offset * 1.7) * 0.65 + ValueNoise(p * 16.3 - offset) * 0.35;
	float level = clamp(0.62 - 0.55 * e + (n - 0.5) * 0.5, 0.0, 1.2);
	float soft = max(fwidth(powder), 0.015);
	float dust = smoothstep(level - soft, level + soft, powder);
	if (dust <= 0.0)
		return rime;
	float pool = ValueNoise(p * 4.3 + offset + 5.7);
	float glaze = smoothstep(0.72, 0.8, pool) * smoothstep(0.35, 0.8, e);
	float rim = (1.0 - smoothstep(0.0, 0.04, abs(pool - 0.72))) * smoothstep(0.35, 0.8, e);
	vec2 cell = floor(p * 13.0 + 300.0);
	uint key = Hash(uint(cell.x) * 7919u + uint(cell.y) * 104729u + seed);
	vec2 spot = (cell + 0.2 + 0.6 * vec2(float((key >> 8) & 255u), float((key >> 16) & 255u)) / 255.0 - 300.0) / 13.0;
	float glintSize = max(0.01, pixel * 0.8);
	float glint = step(0.85, float(key & 255u) / 255.0) * exp(-dot(p - spot, p - spot) / (glintSize * glintSize)) * pow(0.5 + 0.5 * sin(time * 3.0 + float(key >> 24) * 0.16), 6.0) * dust * (1.0 - glaze);
	float thick = clamp(0.35 + 0.65 * e, 0.0, 1.0) * (0.75 + 0.5 * (n - 0.5));
	rime.Coverage = mix(dust * mix(0.5, 0.96, thick), 0.72, glaze);
	rime.Shade = mix(0.86 + 0.2 * thick + 0.1 * rim, 0.8, glaze);
	rime.Roughness = mix(0.75 - 0.2 * thick, 0.05, glaze);
	rime.Glow = glow * (0.05 * dust + 0.1 * rim) + glint * (0.5 + 0.6 * glow);
	return rime;
}

Rime NovaShape(vec2 uv, float extent, float pixel, float glow, float front, float crack, float frost, uint seed, float time)
{
	vec2 p = uv * extent;
	float radius = frost / 0.62;
	float edge = 1.2 * (1.0 - front);
	float r = length(p);
	if (r > radius * (0.71 * front + 0.17))
		return Rime(0.0, 0.0, 1.0, 0.7);
	vec2 offset = vec2(float((seed >> 4) & 255u), float((seed >> 12) & 255u)) * 0.37;
	vec2 heading = p / max(r, 1e-4);
	float reach = radius * 0.62 * front * (0.86 + 0.28 * (ValueNoise(heading * 1.8 + offset) * 0.65 + ValueNoise(heading * 3.7 + offset + 17.1) * 0.35));
	Rime rime = NovaRime(p, reach - r, radius * 0.16, pixel, glow * 0.5, seed, time);
	float count = 12.0 + float(seed % 4u);
	float spin = float((seed >> 8) & 255u) / 255.0 * Tau / count;
	float turn = (atan(p.y, p.x) - spin) / Tau * count;
	float nearest = floor(turn);
	float veins = 0.0;
	for (int i = 0; i < 2; i++)
	{
		float k = i == 0 ? nearest : nearest + (fract(turn) < 0.5 ? -1.0 : 1.0);
		uint key = uint(mod(k, count)) + seed * 31u;
		veins = max(veins, NovaArm(p, (k + 0.5) / count * Tau + spin, reach * 0.97, pixel, key));
	}
	veins *= 1.0 - smoothstep(reach - radius * 0.04, reach + radius * 0.02, r);
	float band = exp(-pow((r - reach) / (radius * 0.05), 2.0)) * edge * step(0.02, front);
	float split = 0.0;
	if (crack > 0.0)
		split = CracksShape(p / max(reach * 1.05, 1e-3), pixel / max(reach, 1e-3), reach, 1.0, seed * 7u + 3u).y / 0.7 * crack;
	rime.Coverage = max(max(rime.Coverage, veins * 0.95 * step(0.0, reach - r + radius * 0.02)), split * 0.9);
	rime.Shade = mix(rime.Shade, 1.12, veins * 0.6) * (1.0 - 0.45 * split);
	rime.Glow += glow * veins * 0.7 + band * (0.6 + 0.4 * ValueNoise(p * 6.0 + offset)) + glow * split * 0.5;
	return rime;
}

float VoidSpiral(vec2 p, float r, float angle, float count, float twist, float spin, float jitter, float width, float pixel, vec2 offset, float time, bool varied, out float flow)
{
	float logr = log(max(r, 0.04));
	float turn = (angle + spin - twist * logr + jitter) / Tau * count;
	float arm = floor(turn + 0.5);
	float distance = abs(turn - arm) / count * Tau * r / sqrt(1.0 + twist * twist);
	float broken = smoothstep(0.32, 0.5, ValueNoise(vec2(logr * 3.2 + offset.x, mod(arm, count) * 7.3 + offset.y)));
	float taper = varied ? mix(1.6, 0.6, ValueNoise(vec2(logr * 5.0 - offset.y, mod(arm, count) * 3.1))) : 1.0;
	flow = 0.35 + 0.65 * pow(0.5 + 0.5 * sin(logr * 9.0 + time * 7.0 + mod(arm, count) * 2.3), 3.0);
	return Line(distance, max(width * taper, pixel * 0.6), pixel) * broken;
}

vec2 VoidFieldShape(vec2 uv, float extent, float pixel, float front, float glow, float spin, float scar, uint seed, float time)
{
	vec2 p = uv * extent;
	float radius = extent / 1.04;
	float r = length(p);
	float n = r / radius;
	if (n > 1.02 || front <= 0.0)
		return vec2(0.0);
	vec2 offset = vec2(float(seed & 255u), float((seed >> 8) & 255u)) * 0.37;
	float angle = atan(p.y, p.x);
	vec2 heading = p / max(r, 1e-4);
	float reach = front * (0.93 + 0.12 * (ValueNoise(heading * 2.6 + offset) - 0.5));
	float inside = 1.0 - smoothstep(reach - 0.14, reach, n);
	float mottled = ValueNoise(p * 1.25 + offset);
	float dark = inside * (0.3 + 0.62 * pow(max(1.0 - n / max(reach, 1e-3), 0.0), 1.3)) * (0.75 + 0.5 * mottled);
	if (scar > 0.0)
	{
		vec2 cracks = CracksShape(p / (radius * 0.9), pixel / (radius * 0.9), radius * 0.9, 1.0, seed * 5u + 1u);
		return vec2(max(dark * 0.7, cracks.x * 0.8 * inside), cracks.y * 0.9 * glow * inside);
	}
	float jitter = 0.45 * (ValueNoise(p * 2.8 + offset) - 0.5);
	float flowA;
	float flowB;
	float major = VoidSpiral(p, r, angle, 5.0, 1.35, spin, jitter, 0.04, pixel, offset, time, true, flowA);
	float minor = VoidSpiral(p, r, angle, 9.0, 2.6, spin * 1.35 + 1.7, jitter * 1.4, 0.018, pixel, offset + 9.1, time * 1.3, false, flowB);
	float depth = smoothstep(0.04, 0.2, n) * inside;
	float fissures = (major * flowA + minor * flowB * 0.55) * depth;
	float rim = Line(abs(n - reach) * radius, 0.035 + 0.02 * ValueNoise(heading * 9.0 + offset), pixel) * step(0.05, front);
	float wisps = pow(ValueNoise(vec2((angle + spin * 0.6) / Tau * 40.0, n * 6.0 + time * 1.5) + offset), 6.0) * (1.0 - smoothstep(0.0, 0.18, reach - n)) * inside;
	float coverage = clamp(dark + (major + minor * 0.5) * depth * 0.5, 0.0, 0.94);
	return vec2(coverage, glow * (fissures * 1.1 + rim * 0.22 + wisps * 0.6));
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
