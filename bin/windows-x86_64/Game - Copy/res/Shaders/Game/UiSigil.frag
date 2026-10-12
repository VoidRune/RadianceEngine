#version 450
#include "UiCommon.glsl"

const uint SigilRings = 1u;
const uint SigilRunes = 2u;
const uint SigilStar = 4u;
const uint SigilCore = 8u;

float Segment(vec2 p, vec2 a, vec2 b)
{
	vec2 pa = p - a;
	vec2 ba = b - a;
	float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
	return length(pa - ba * h);
}

vec2 Rotate(vec2 v, vec2 cs)
{
	return vec2(v.x * cs.x - v.y * cs.y, v.x * cs.y + v.y * cs.x);
}

vec4 RuneStroke(uint bits)
{
	float side = (bits & 1u) != 0u ? 1.0 : -1.0;
	float start = float((bits >> 1u) & 3u) * 0.66 - 1.0;
	float rise = (bits & 8u) != 0u ? 0.65 : -0.65;
	return vec4(0.0, start, side, clamp(start + rise, -1.0, 1.0));
}

void main()
{
	Primitive primitive = primitives[inIndex];
	float pixel = 1.0 / pc.Scale;
	float radius = primitive.Rect.z;
	vec2 p = inLocal;
	float r = length(p);
	if (r > radius + 2.0 * pixel)
	{
		outColor = vec4(0.0);
		return;
	}
	vec4 color = unpackUnorm4x8(primitive.Colors.x);
	vec4 accent = unpackUnorm4x8(primitive.Colors.y);
	vec4 disc = unpackUnorm4x8(primitive.Colors.z);
	float angle = atan(p.y, p.x);
	float time = pc.Time * primitive.Params.x + primitive.Params.y * 17.0;
	float width = max(primitive.Params.z, 0.25);
	uint layers = primitive.Info.z >> 2;
	uint seed = uint(primitive.Params.y * 1000.0);
	float halo = 3.0 + 0.012 * radius;
	float reach = 3.5 * halo + width;
	float nearest = 1e6;
	float runeNearest = 1e6;

	if ((layers & SigilRings) != 0u && r > radius * 0.885 - reach)
	{
		nearest = min(nearest, min(abs(r - radius * 0.985), abs(r - radius * 0.94)));
		float ticks = radius > 60.0 ? 72.0 : (radius > 25.0 ? 36.0 : 18.0);
		float cell = (angle + time * 0.06) / TwoPi * ticks;
		float index = floor(cell + 0.5);
		float along = abs(cell - index) * TwoPi / ticks * r;
		float low = radius * (mod(index, 6.0) < 0.5 ? 0.885 : 0.94);
		nearest = min(nearest, max(along, max(low - r, r - radius * 0.985)));
	}
	if ((layers & SigilRunes) != 0u && abs(r - radius * 0.78) < radius * 0.08 + reach)
	{
		float low = radius * 0.7;
		float high = radius * 0.86;
		nearest = min(nearest, min(abs(r - low), abs(r - high)));
		float middle = 0.5 * (low + high);
		float band = high - low;
		float cells = max(floor(TwoPi * middle / (band * 1.05)), 6.0);
		float cell = (angle - time * 0.035) / TwoPi * cells;
		uint h = HashU(uint(mod(floor(cell), cells)) * 747796405u + seed * 2891336453u + 12345u);
		float cellWidth = TwoPi * r / cells;
		vec2 q = vec2((fract(cell) - 0.5) * cellWidth, r - middle);
		vec2 size = vec2(min(cellWidth, band) * 0.26, band * 0.3);
		float d = Segment(q, vec2(0.0, -size.y), vec2(0.0, size.y));
		for (uint k = 0u; k < 3u; k++)
		{
			vec4 stroke = RuneStroke(h >> (k * 5u));
			d = min(d, Segment(q, stroke.xy * size, stroke.zw * size));
		}
		if (((h >> 20u) & 7u) == 0u)
			d = abs(length(q) - size.y * 0.35);
		runeNearest = min(runeNearest, d);
	}
	if ((layers & SigilStar) != 0u && r < radius * 0.68 + reach)
	{
		float starRadius = radius * 0.64;
		nearest = min(nearest, abs(r - starRadius));
		float count = clamp(primitive.Shape.y, 3.0, 12.0);
		float step = TwoPi / count;
		vec2 turn = vec2(cos(step), sin(step));
		vec2 jump = vec2(cos(step * primitive.Shape.z), sin(step * primitive.Shape.z));
		float start = -time * 0.045 + primitive.Params.y * 3.0;
		vec2 v = vec2(cos(start), sin(start)) * starRadius;
		for (int j = 0; j < 12; j++)
		{
			if (float(j) >= count)
				break;
			nearest = min(nearest, Segment(p, v, Rotate(v, jump)));
			runeNearest = min(runeNearest, abs(length(p - v) - radius * 0.035));
			v = Rotate(v, turn);
		}
	}
	if ((layers & SigilCore) != 0u && r < radius * 0.3 + reach)
	{
		nearest = min(nearest, min(abs(r - radius * 0.3), abs(r - radius * 0.255)));
		float start = time * 0.08;
		vec2 turn = vec2(-0.5, 0.8660254);
		vec2 v = vec2(cos(start), sin(start)) * radius * 0.255;
		for (int j = 0; j < 6; j++)
		{
			runeNearest = min(runeNearest, Segment(p, v, Rotate(v, vec2(-0.5, -0.8660254))));
			v = Rotate(v, turn);
		}
		runeNearest = min(runeNearest, abs(r - radius * 0.07));
	}

	float reveal = 1.0;
	if (primitive.Shape.x < 1.0)
	{
		float sweep = fract(angle / TwoPi + 1.25);
		reveal = clamp((primitive.Shape.x - sweep) * 40.0, 0.0, 1.0);
	}
	float breath = 0.82 + 0.18 * sin(time * 0.9);
	float line = clamp((0.5 * width - nearest) / pixel + 0.5, 0.0, 1.0);
	float rune = clamp((0.5 * width - runeNearest) / pixel + 0.5, 0.0, 1.0);
	float glow = primitive.Params.w * breath * (0.55 * exp(-nearest / halo) + 0.45 * exp(-runeNearest / halo));
	vec4 strokes = Premultiply(color) * line;
	strokes += Premultiply(accent) * rune * (1.0 - strokes.a);
	vec4 result = strokes + Premultiply(disc) * Coverage(r - radius, pixel) * (1.0 - strokes.a);
	result.rgb += mix(color.rgb, accent.rgb, 0.5) * glow * max(color.a, accent.a);
	outColor = Finish(primitive, result * reveal * Coverage(r - radius - 2.0 * pixel, pixel));
}
