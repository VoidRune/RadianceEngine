#version 450
#include "UiCommon.glsl"

const uint RarityRare = 1u;
const uint RarityLegendary = 2u;
const uint RarityApex = 3u;
const float Unbounded = 4096.0;
const float BurnWidth = 0.07;

float Lattice(vec2 cell)
{
	vec3 p = fract(vec3(cell.xyx) * 0.1031);
	p += dot(p, p.yzx + 33.33);
	return fract((p.x + p.y) * p.z);
}

float Noise(vec2 p, float period)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float x0 = mod(i.x, period);
	float x1 = mod(i.x + 1.0, period);
	return mix(mix(Lattice(vec2(x0, i.y)), Lattice(vec2(x1, i.y)), f.x), mix(Lattice(vec2(x0, i.y + 1.0)), Lattice(vec2(x1, i.y + 1.0)), f.x), f.y);
}

float Fbm(vec2 p, float period)
{
	float value = 0.5 * Noise(p, period);
	value += 0.25 * Noise(p * 2.0 + vec2(0.0, 5.3), period * 2.0);
	value += 0.125 * Noise(p * 4.0 + vec2(0.0, 9.1), period * 4.0);
	return value / 0.875;
}

vec3 Prism(float t)
{
	return clamp(0.6 + 0.42 * cos(TwoPi * (t + vec3(0.0, 0.34, 0.67))), 0.0, 1.0);
}

vec3 Fire(float heat)
{
	if (heat < 0.35)
		return mix(vec3(0.72, 0.27, 0.03), vec3(1.0, 0.6, 0.11), heat / 0.35);
	if (heat < 0.7)
		return mix(vec3(1.0, 0.6, 0.11), vec3(1.0, 0.83, 0.36), (heat - 0.35) / 0.35);
	return mix(vec3(1.0, 0.83, 0.36), vec3(1.0, 0.97, 0.84), (heat - 0.7) / 0.3);
}

float Bound(vec2 p, vec2 e, float c)
{
	vec2 q = abs(p);
	return max(max(q.x - e.x, q.y - e.y), (q.x + q.y - e.x - e.y + c) * 0.70710678);
}

float CardField(vec2 p, vec2 e, float c, out float s, out vec2 normal)
{
	vec2 q = abs(p);
	float diagonal = c * 1.41421356;
	vec2 b = vec2(e.x - c, e.y);
	vec2 a = vec2(e.x, e.y - c);
	vec2 h = vec2(clamp(q.x, 0.0, b.x), e.y);
	vec2 ab = a - b;
	float t = clamp(dot(q - b, ab) / dot(ab, ab), 0.0, 1.0);
	vec2 g = b + ab * t;
	vec2 v = vec2(e.x, clamp(q.y, 0.0, a.y));
	float dist = length(q - h);
	vec2 nearest = h;
	vec2 face = vec2(0.0, 1.0);
	s = h.x;
	if (length(q - g) < dist)
	{
		dist = length(q - g);
		nearest = g;
		face = vec2(0.70710678);
		s = b.x + t * diagonal;
	}
	if (length(q - v) < dist)
	{
		dist = length(q - v);
		nearest = v;
		face = vec2(1.0, 0.0);
		s = b.x + diagonal + a.y - v.y;
	}
	float inside = Bound(p, e, c);
	normal = (inside < 0.0 || dist < 1e-4 ? face : (q - nearest) / dist) * vec2(p.x < 0.0 ? -1.0 : 1.0, p.y < 0.0 ? -1.0 : 1.0);
	float quarter = b.x + diagonal + a.y;
	s = p.y < 0.0 ? (p.x >= 0.0 ? s : 4.0 * quarter - s) : (p.x >= 0.0 ? 2.0 * quarter - s : 2.0 * quarter + s);
	return inside < 0.0 ? inside : dist;
}

float Glint(float s, float perimeter, float head, float width)
{
	float a = mod(s - head + 0.5 * perimeter, perimeter) - 0.5 * perimeter;
	return exp(-a * a / (2.0 * width * width));
}

float Sheen(vec2 p, vec2 e, float time, float period, float width)
{
	float x = (p.x + p.y * 0.5) / (e.x + e.y * 0.5);
	float center = mix(-1.5, 1.5, clamp(fract(time / period) / 0.3, 0.0, 1.0));
	return exp(-(x - center) * (x - center) / (width * width));
}

float Twinkles(vec2 p, float cell, float time, float seed, float density)
{
	vec2 id = floor(p / cell);
	float h = Lattice(id + vec2(seed * 61.0, seed * 17.0));
	if (h > density)
		return 0.0;
	vec2 v = p - (id + 0.5) * cell - (vec2(fract(h * 23.17), fract(h * 41.31)) - 0.5) * cell * 0.5;
	float pulse = pow(max(sin(time * (0.9 + 1.7 * fract(h * 7.7)) + h * 61.0), 0.0), 6.0);
	float arm = cell * 0.24;
	float streaks = exp(-abs(v.y) * 1.3) * max(1.0 - abs(v.x) / arm, 0.0) + exp(-abs(v.x) * 1.3) * max(1.0 - abs(v.y) / arm, 0.0);
	return (exp(-dot(v, v) * 0.45) + streaks * 0.55) * pulse;
}

vec3 Flames(vec2 p, float outside, vec2 normal, float time)
{
	float up = -normal.y;
	float reach = 12.0 + 30.0 * max(up, 0.0) + 8.0 * (1.0 - abs(up));
	if (outside > reach * 1.8)
		return vec3(0.0);
	float warp = Noise(vec2(p.x * 0.02, p.y * 0.016 + time * 0.6), Unbounded);
	float n = Fbm(vec2(p.x * 0.065 + warp * 1.6, p.y * 0.026 + time * 1.35), Unbounded);
	float heat = n * 1.5 - outside / reach - 0.2;
	float glow = clamp(heat + 0.35, 0.0, 1.0);
	return Fire(clamp(heat * 1.35, 0.0, 1.0)) * (smoothstep(0.0, 0.32, heat) * 0.95 + glow * glow * 0.12) + vec3(1.0, 0.86, 0.45) * exp(-outside / 3.5) * 0.35;
}

float Embers(vec2 p, float d, float up, float time, float seed)
{
	if (d < -4.0 || d > 72.0 || up < -0.3)
		return 0.0;
	float sparks = 0.0;
	for (int layer = 0; layer < 2; layer++)
	{
		float l = float(layer);
		vec2 size = vec2(24.0 + 9.0 * l, 58.0 + 14.0 * l);
		vec2 q = vec2(p.x + l * 11.0, p.y + time * (30.0 + 16.0 * l));
		vec2 id = floor(q / size);
		float h = Lattice(id + vec2(seed * 47.0 + l * 5.0, l * 9.0));
		if (h > 0.62)
			continue;
		vec2 v = q - id * size - vec2(size.x * (0.25 + 0.5 * fract(h * 17.3)) + 4.0 * sin(time * 2.1 + h * 40.0), size.y * fract(h * 29.7));
		v.y *= 0.45;
		sparks += exp(-dot(v, v) * 0.55) * (0.55 + 0.45 * sin(time * 8.0 + h * 90.0));
	}
	return sparks * smoothstep(-2.0, 8.0, d) * exp(-max(d, 0.0) / 30.0) * smoothstep(-0.3, 0.6, up);
}

vec3 MoltenGold(float u, float v, float time, float period)
{
	float n = Fbm(vec2(u - time * 1.5, v * 0.25 + time * 0.3), period);
	float bevel = 1.0 - v * v;
	vec3 color = mix(vec3(0.34, 0.16, 0.02), vec3(0.98, 0.68, 0.2), smoothstep(0.15, 0.6, n * (0.45 + 0.55 * bevel)));
	color = mix(color, vec3(1.0, 0.95, 0.7), smoothstep(0.55, 0.85, n) * bevel);
	return color + vec3(1.0, 0.9, 0.6) * pow(bevel, 6.0) * 0.25;
}

vec4 Gem(vec2 p, float size, float pixel, bool prismatic, float time)
{
	float d = (abs(p.x) + abs(p.y) - size) * 0.70710678;
	float coverage = Coverage(d, pixel);
	if (coverage <= 0.0)
		return vec4(0.0);
	float facet = p.x < 0.0 ? (p.y < 0.0 ? 0.0 : 0.5) : (p.y < 0.0 ? 0.25 : 0.75);
	float light = 1.0 - facet * 0.7;
	vec3 base = prismatic ? mix(Prism(time * 0.15 + facet), vec3(1.0), 0.25) : mix(vec3(0.75, 0.42, 0.06), vec3(1.0, 0.86, 0.45), light);
	float table = Coverage(abs(p.x) + abs(p.y) - size * 0.42, pixel);
	vec3 color = base * mix(0.55 + 0.45 * light, 1.2, table);
	color = mix(vec3(0.06, 0.03, 0.08), color, Coverage(d + 1.1, pixel));
	vec2 spark = p + vec2(size * 0.28, size * 0.28);
	color += vec3(1.0) * exp(-dot(spark, spark) * 0.35) * (0.6 + 0.4 * sin(time * 3.1));
	return vec4(color, 1.0) * coverage;
}

void main()
{
	Primitive primitive = primitives[inIndex];
	float pixel = 1.0 / pc.Scale;
	vec2 p = inLocal;
	vec2 extent = primitive.Shape.xy;
	float chamfer = primitive.Shape.z;
	uint rarity = primitive.Info.y;
	float seed = float(primitive.Info.z) / 1023.0;
	float hover = primitive.Params.x;
	float fade = primitive.Params.y;
	float taken = primitive.Params.z;
	float reveal = primitive.Params.w;
	vec3 color = unpackUnorm4x8(primitive.Colors.x).rgb;
	float time = mod(pc.Time, 3600.0) + seed * 97.0;

	float s;
	vec2 normal;
	float d = CardField(p, extent, chamfer, s, normal);
	float outside = max(d, 0.0);
	float perimeter = 4.0 * (extent.x + extent.y + chamfer * (1.41421356 - 2.0));
	float u = s / perimeter;
	float show = smoothstep(0.0, 0.35, reveal);
	float flash = (1.0 - reveal) * smoothstep(0.0, 0.2, reveal);
	float energy = (0.8 + 0.55 * hover) * (0.25 + 0.75 * fade);
	vec2 room = primitive.Rect.zw - abs(p);
	float window = smoothstep(0.0, 18.0, min(room.x, room.y));

	vec3 aura = vec3(0.0);
	if (d > -4.0)
	{
		if (rarity == RarityApex)
		{
			float angle = atan(p.y, p.x) / TwoPi;
			aura = Prism(angle + time * 0.06) * (exp(-outside / 12.0) * 0.55 + exp(-outside / 32.0) * 0.18);
			float rays = Noise(vec2(angle * 40.0 - time * 0.5, time * 0.35), 40.0);
			aura += mix(Prism(angle * 2.0 - time * 0.09), vec3(1.0), 0.25) * pow(rays, 4.0) * smoothstep(0.0, 10.0, d) * exp(-outside / 48.0) * 1.2;
			aura += mix(Prism(u * 3.0 + seed), vec3(1.0), 0.55) * Twinkles(p, 21.0, time, seed, 0.32) * smoothstep(2.0, 9.0, d) * exp(-outside / 44.0) * 1.5;
		}
		else if (rarity == RarityLegendary)
		{
			aura = vec3(1.0, 0.72, 0.26) * (exp(-outside / 13.0) * 0.42 + exp(-outside / 32.0) * 0.12);
			aura += Flames(p, outside, normal, time) * (0.9 + 0.3 * hover);
			aura += vec3(1.0, 0.82, 0.46) * Embers(p, d, -normal.y, time, seed) * 1.3;
		}
		else if (rarity == RarityRare)
		{
			float cells = max(floor(perimeter / 44.0), 1.0);
			vec2 w = vec2(u * cells - time * 0.25, outside * 0.05 - time * 0.4);
			float wisp = (Noise(w, cells) * 2.0 + Noise(w * 2.0 + vec2(0.0, 5.3), cells * 2.0)) / 3.0;
			aura = color * (exp(-outside / 11.0) * 0.38 + exp(-outside / 28.0) * 0.1);
			aura += mix(color, vec3(0.52, 0.72, 1.0), wisp) * smoothstep(0.44, 0.84, wisp) * exp(-outside / 20.0) * 0.95;
			aura += mix(color, vec3(1.0), 0.6) * Twinkles(p, 25.0, time, seed, 0.16) * smoothstep(2.0, 9.0, d) * exp(-outside / 36.0);
		}
		else
			aura = color * (exp(-outside / 9.0) * 0.32 + exp(-outside / 24.0) * 0.07) * (0.85 + 0.15 * sin(time * 1.6));
		aura *= energy;
		aura += mix(color, vec3(1.0), 0.5) * flash * exp(-outside / 18.0) * 1.6;
		if (taken > 0.0)
			aura += mix(color, vec3(1.0), 0.45) * (taken * exp(-abs(d - (1.0 - taken) * 110.0) / 6.0) * 2.2 + taken * taken * exp(-outside / 30.0) * 1.2);
	}

	float shadow = 0.55 * (1.0 - smoothstep(-14.0, 24.0, Bound(p - vec2(0.0, 9.0), extent, chamfer))) * (0.6 + 0.4 * fade);
	vec4 result = vec4(aura * show, shadow * show) * window;

	float coverage = Coverage(d, pixel);
	if (coverage > 0.0)
	{
		float inner = exp(min(d, 0.0) / 24.0);
		vec3 face = mix(vec3(0.14, 0.105, 0.27), vec3(0.055, 0.04, 0.115), clamp(p.y / extent.y * 0.5 + 0.5, 0.0, 1.0));
		face += color * inner * 0.07;
		if (rarity == RarityApex)
		{
			vec2 view = clamp(primitive.Uv.xy / extent, -1.5, 1.5) * hover + vec2(sin(time * 0.37), cos(time * 0.29)) * 0.6 * (1.0 - hover);
			vec2 q = p / extent;
			vec2 drift = p * 0.0065 + vec2(time * 0.025, -time * 0.018);
			float swirl = (Noise(drift, Unbounded) * 2.0 + Noise(drift * 2.1 + vec2(0.0, 5.3), Unbounded)) / 3.0;
			vec2 r = vec2(p.x * 0.866 + p.y * 0.5, p.y * 0.866 - p.x * 0.5) / 34.0;
			vec2 f = fract(r);
			float h = Lattice(floor(r) * 1.31 + (f.x > f.y ? vec2(7.0, 3.0) : vec2(0.0)) + seed * 13.0);
			float film = swirl * 1.4 + dot(q, vec2(0.35, 0.5)) - dot(view, vec2(0.3, 0.22)) + time * 0.03 + h * 0.06;
			float glare = exp(-pow(dot(q, vec2(0.8, 0.6)) - view.x * 0.6 - view.y * 0.35, 2.0) / 0.08);
			face += mix(Prism(film), vec3(1.0), 0.18) * (0.08 + 0.12 * inner + 0.06 * hover + glare * 0.16) * fade;
			face += mix(Prism(u * 2.0 - time * 0.1), vec3(1.0), 0.3) * Sheen(p, extent, time, 3.2, 0.16) * 0.2 * fade;
		}
		else if (rarity == RarityLegendary)
		{
			face += vec3(1.0, 0.5, 0.12) * exp(min(d, 0.0) / 15.0) * (0.2 + 0.12 * Noise(vec2(u * 60.0, time * 2.2), 60.0)) * fade;
			face += vec3(1.0, 0.86, 0.5) * Sheen(p, extent, time, 4.0, 0.14) * 0.16 * fade;
			face += vec3(1.0, 0.85, 0.5) * Twinkles(p, 27.0, time, seed, 0.12) * 0.45 * fade;
		}
		else if (rarity == RarityRare)
		{
			face += color * Noise(p * 0.016 + vec2(0.0, -time * 0.06), Unbounded) * inner * 0.16 * fade;
			face += mix(color, vec3(1.0), 0.5) * Sheen(p, extent, time, 5.0, 0.13) * 0.1 * fade;
		}
		else
		{
			face += mix(color, vec3(1.0), 0.4) * Sheen(p, extent, time, 7.0, 0.12) * 0.06 * fade;
			face += color * Twinkles(p, 30.0, time * 0.7, seed, 0.1) * 0.25 * fade;
		}
		vec2 toMouse = p - primitive.Uv.xy;
		face += color * exp(-dot(toMouse, toMouse) / 12800.0) * 0.08 * hover;
		face += mix(color, vec3(1.0), 0.5) * (taken * taken * 0.4 + flash * 0.3);
		float faceAlpha = 0.97 * (0.8 + 0.2 * fade) * show * coverage;
		result = vec4(face, 1.0) * faceAlpha + result * (1.0 - faceAlpha);
	}

	float width = (rarity >= RarityLegendary ? 4.2 : 2.2) + 0.5 * hover;
	float band = Coverage(d, pixel) * Coverage(-d - width, pixel);
	if (band > 0.0)
	{
		float v = clamp(-d / width * 2.0 - 1.0, -1.0, 1.0);
		float bevel = 1.0 - v * v;
		vec3 frame;
		if (rarity == RarityApex)
		{
			frame = mix(Prism(u * 2.0 - time * 0.22), vec3(1.0), 0.28 + 0.3 * bevel);
			frame += vec3(0.8) * (Glint(s, perimeter, time * 160.0, 22.0) + Glint(s, perimeter, perimeter * 0.5 - time * 110.0, 16.0));
		}
		else if (rarity == RarityLegendary)
		{
			float cells = max(floor(perimeter / 30.0), 1.0);
			frame = MoltenGold(u * cells, v, time, cells);
			frame += vec3(1.0, 0.95, 0.75) * Glint(s, perimeter, time * 140.0 + seed * perimeter, 20.0) * bevel * 0.9;
		}
		else
		{
			frame = color * (0.62 + 0.38 * bevel);
			float heads = Glint(s, perimeter, time * (rarity == RarityRare ? 90.0 : 55.0) + seed * perimeter, 40.0);
			if (rarity == RarityRare)
				heads += Glint(s, perimeter, perimeter * 0.5 - time * 70.0, 34.0);
			frame = mix(frame, vec3(1.0), clamp(heads, 0.0, 1.0) * 0.55);
		}
		frame *= 0.85 + 0.25 * hover + 0.6 * taken + flash;
		float alpha = band * show * (0.75 + 0.25 * fade);
		result = vec4(frame, 1.0) * alpha + result * (1.0 - alpha);
	}

	if (rarity == RarityLegendary)
	{
		float vein = Coverage(abs(d + 9.0) - 0.8, pixel) * coverage * show * (0.55 + 0.45 * fade);
		if (vein > 0.0)
		{
			float cells = max(floor(perimeter / 26.0), 1.0);
			result = vec4(MoltenGold(u * cells + 0.5, 0.0, time * 0.7 + 3.0, cells), 1.0) * vein + result * (1.0 - vein);
		}
	}
	else
	{
		float line = Coverage(abs(d + 9.0) - 0.5, pixel) * coverage;
		vec3 lineColor = rarity == RarityApex ? mix(Prism(u * 2.0 + time * 0.1), vec3(1.0), 0.4) : color;
		result.rgb += lineColor * line * (0.22 + 0.1 * hover) * show * fade;
	}

	if (rarity >= RarityLegendary)
	{
		bool prismatic = rarity == RarityApex;
		vec2 crown = p + vec2(0.0, extent.y);
		vec2 heel = p - vec2(0.0, extent.y);
		vec3 shine = prismatic ? mix(Prism(time * 0.15), vec3(1.0), 0.3) : vec3(1.0, 0.76, 0.32);
		result.rgb += shine * (exp(-max(length(crown) - 9.0, 0.0) / 6.0) + exp(-max(length(heel) - 6.0, 0.0) / 5.0) * 0.6) * 0.35 * energy * show * window;
		vec4 gem = (Gem(crown, 10.0 + 1.5 * hover, pixel, prismatic, time) + Gem(heel, 7.0, pixel, prismatic, time + 1.7)) * show;
		result = gem + result * (1.0 - gem.a);
	}

	float radius = primitive.Uv.w;
	vec2 m = p - vec2(0.0, primitive.Uv.z);
	float r = length(m);
	if (radius > 0.0 && r < radius + 30.0)
	{
		float angle = atan(m.y, m.x + 1e-5) / TwoPi + 0.5;
		vec3 ring = color;
		if (rarity == RarityApex)
			ring = mix(Prism(angle + time * 0.12), vec3(1.0), 0.3);
		else if (rarity == RarityLegendary)
		{
			float cells = max(floor(TwoPi * radius / 26.0), 1.0);
			ring = MoltenGold(angle * cells, 0.0, time, cells);
		}
		float disc = Coverage(r - radius, pixel) * show;
		vec3 well = mix(vec3(0.035, 0.025, 0.075), ring * 0.28, smoothstep(radius * 0.2, radius, r));
		result = vec4(well, 1.0) * (disc * 0.94) + result * (1.0 - disc * 0.94);
		float arcs = 1.0;
		if (rarity == RarityRare)
			arcs = 0.5 + 0.5 * max(cos(TwoPi * (2.0 * angle + time * 0.2)), 0.0);
		else if (rarity < RarityRare)
			arcs = 0.55 + 0.45 * max(cos(TwoPi * (angle - time * 0.12)), 0.0);
		float stroke = Coverage(abs(r - radius) - (rarity >= RarityLegendary ? 1.3 : 0.75), pixel) * show;
		result = vec4(ring * arcs, 1.0) * stroke + result * (1.0 - stroke);
		result.rgb += ring * exp(-max(r - radius, 0.0) / 9.0) * (1.0 - disc) * 0.22 * energy * show;
	}
	float burnAway = uintBitsToFloat(primitive.Colors.y);
	if (burnAway > 0.0)
	{
		float field = mix(Fbm(p * 0.02 + seed * 37.0, Unbounded), clamp(0.5 + p.y / (extent.y * 2.4), 0.0, 1.0), 0.4);
		float threshold = mix(-BurnWidth, 1.0, burnAway);
		float soft = max(fwidth(field), 1e-4);
		float keep = smoothstep(threshold - soft, threshold + soft, field);
		float edge = 1.0 - smoothstep(threshold, threshold + BurnWidth, field);
		float charred = 1.0 - smoothstep(threshold, threshold + BurnWidth * 3.0, field);
		float body = clamp(result.a, 0.0, 1.0);
		result.rgb *= 1.0 - 0.75 * charred;
		result *= keep;
		result.rgb += mix(vec3(1.0, 0.52, 0.14), color, 0.4) * (edge * edge * keep * body * 1.8);
		result.a = max(result.a, edge * keep * body);
	}

	outColor = Finish(primitive, result);
}
