#version 450
#extension GL_EXT_nonuniform_qualifier : require
#include "UiCommon.glsl"

layout(set = 0, binding = 1) uniform sampler2D Textures[8];
layout(set = 0, binding = 2) uniform sampler2D BackdropLight;

const uint ModeRect = 0u;
const uint ModeGlyph = 1u;
const uint ModeImage = 2u;
const uint ModeOrb = 3u;
const uint ModeTriangle = 4u;
const uint FlagHorizontal = 1u;
const uint FlagShimmer = 2u;

float Median(float a, float b, float c)
{
	return max(min(a, b), min(max(a, b), c));
}

float Lattice(vec2 cell)
{
	vec3 p = fract(vec3(cell.xyx) * 0.1031);
	p += dot(p, p.yzx + 33.33);
	return fract((p.x + p.y) * p.z);
}

float Noise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(Lattice(i), Lattice(i + vec2(1.0, 0.0)), f.x), mix(Lattice(i + vec2(0.0, 1.0)), Lattice(i + vec2(1.0, 1.0)), f.x), f.y);
}

float Fbm(vec2 p)
{
	float value = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < 3; i++)
	{
		value += amplitude * Noise(p);
		p = p * 2.07 + vec2(13.7, 7.1);
		amplitude *= 0.5;
	}
	return value / 0.875;
}

float RoundBox(vec2 p, vec2 extent, float radius)
{
	radius = min(radius, min(extent.x, extent.y));
	vec2 q = abs(p) - extent + radius;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float Frame(vec2 p, vec2 extent, float radius, float chamfer)
{
	float d = RoundBox(p, extent, radius);
	if (chamfer > 0.0)
	{
		chamfer = min(chamfer, min(extent.x, extent.y));
		vec2 q = abs(p);
		d = max(d, (q.x + q.y - (extent.x + extent.y - chamfer)) * 0.70710678);
	}
	return d;
}

float Triangle(vec2 p, vec2 q)
{
	p.x = abs(p.x);
	vec2 a = p - q * clamp(dot(p, q) / dot(q, q), 0.0, 1.0);
	vec2 b = p - q * vec2(clamp(p.x / q.x, 0.0, 1.0), 1.0);
	float s = -sign(q.y);
	vec2 d = min(vec2(dot(a, a), s * (p.x * q.y - p.y * q.x)), vec2(dot(b, b), s * (p.y - q.y)));
	return -sqrt(d.x) * sign(d.y);
}

vec3 Aether(vec2 position, vec3 tint, float seed, float stars)
{
	float t = pc.Time * 0.03;
	vec2 q = position / 240.0 + vec2(seed * 3.7, seed * 1.9);
	float warp = Fbm(q + vec2(t, -0.6 * t));
	float n = Fbm(q * 1.6 + vec2(warp, 1.0 - warp) * 1.8 - vec2(0.0, t));
	float wisps = smoothstep(0.5, 0.95, n);
	float sparkle = Lattice(floor(position / 5.0) + vec2(floor(seed * 97.0), 0.0));
	vec2 f = fract(position / 5.0) - 0.5;
	float twinkle = 0.5 + 0.5 * sin(pc.Time * (1.2 + 2.5 * fract(sparkle * 13.0)) + sparkle * 50.0);
	float star = step(0.995, sparkle) * smoothstep(0.32, 0.0, length(f)) * twinkle;
	return tint * wisps * 0.2 + mix(tint, vec3(1.0), 0.6) * star * 0.5 * stars;
}

float PerimeterCoordinate(vec2 p, vec2 e)
{
	if (e.y - abs(p.y) < e.x - abs(p.x))
		return p.y < 0.0 ? p.x + e.x : 3.0 * e.x + 2.0 * e.y - p.x;
	return p.x > 0.0 ? 2.0 * e.x + e.y + p.y : 4.0 * e.x + 3.0 * e.y - p.y;
}

float Flow(vec2 p, vec2 e, float seed)
{
	float perimeter = 4.0 * (e.x + e.y);
	float s = PerimeterCoordinate(p, e);
	float head = mod(pc.Time * 38.0 + seed * perimeter, perimeter);
	float tail = clamp(perimeter * 0.22, 50.0, 240.0);
	float a = mod(head - s, perimeter);
	return exp(-a / tail) * smoothstep(0.0, 40.0, a);
}

float RimVeins(float angle, float r, float pixel, float salt, float time, float reachScale)
{
	float d = 1.0 - r;
	float span = max(r, 0.05);
	float vein = 0.0;
	for (int i = 0; i < 14; i++)
	{
		float fi = float(i);
		float h1 = fract(sin(fi * 12.9898 + salt) * 43758.5453);
		float h2 = fract(sin(fi * 78.233 + salt * 1.7) * 24634.6345);
		float h3 = fract(sin(fi * 39.425 + salt * 2.3) * 14375.8573);
		float reach = (0.22 + 0.38 * h2) * reachScale;
		if (d > reach + 0.02)
			continue;
		float t = clamp(d / reach, 0.0, 1.0);
		float a = (fi + 0.8 * h1) * 0.44879895;
		float meander = (0.75 * sin(d * (3.0 + 2.0 * h3) + fi * 2.1 + time * 0.2) + 0.12 * sin(d * 13.0 + fi * 5.3 - time * 0.15)) * d;
		float diff = abs(mod(angle - a - meander + 3.14159265, 6.2831853) - 3.14159265) * span;
		float w = (0.022 + 0.014 * h3) * pow(1.0 - t, 0.9) + 0.002;
		vein = max(vein, (1.0 - smoothstep(w - pixel, w + pixel, diff)) * (0.65 + 0.35 * h3));
		float start = reach * (0.3 + 0.3 * h1);
		float stretch = reach * (0.4 + 0.3 * h2);
		float s = d - start;
		if (s >= 0.0 && s <= stretch)
		{
			float side = h3 > 0.5 ? 1.0 : -1.0;
			float u = s / stretch;
			float bend = a + meander + side * (0.55 * s * (1.0 - 0.6 * u) + 0.06 * sin(s * 9.0 + fi));
			float bd = abs(mod(angle - bend + 3.14159265, 6.2831853) - 3.14159265) * span;
			float bw = 0.6 * w * pow(1.0 - u, 0.8) + 0.0015;
			vein = max(vein, (1.0 - smoothstep(bw - pixel, bw + pixel, bd)) * 0.75);
		}
	}
	return vein;
}

void main()
{
	Primitive primitive = primitives[inIndex];

	uint mode = primitive.Info.x;
	float pixel = 1.0 / pc.Scale;
	vec2 extent = primitive.Rect.zw;
	vec4 top = unpackUnorm4x8(primitive.Colors.x);
	vec4 bottom = unpackUnorm4x8(primitive.Colors.y);
	vec4 border = unpackUnorm4x8(primitive.Colors.z);
	vec4 extra = unpackUnorm4x8(primitive.Colors.w);
	float along = (primitive.Info.z & FlagHorizontal) != 0u ? inLocal.x / max(extent.x, 1e-4) : inLocal.y / max(extent.y, 1e-4);
	vec4 fill = mix(top, bottom, clamp(along * 0.5 + 0.5, 0.0, 1.0));
	vec4 result = vec4(0.0);

	if (mode == ModeRect)
	{
		float d = Frame(inLocal, extent, primitive.Shape.x, primitive.Uv.x);
		float softness = primitive.Shape.z;
		float coverage = softness > 0.0 ? 1.0 - smoothstep(-softness, softness, d) : Coverage(d, pixel);
		if (primitive.Params.w > 0.0 && pc.Backdrop > 0.5)
		{
			vec3 environment = texture(BackdropLight, inScreen / pc.ScreenSize).rgb;
			float glance = dot(environment, vec3(0.3, 0.55, 0.15));
			border.rgb = border.rgb * (0.6 + 1.6 * glance * primitive.Params.w) + environment * 0.5 * primitive.Params.w;
			border.a = min(border.a * (0.75 + 1.2 * glance * primitive.Params.w), 1.0);
		}
		if (primitive.Uv.z > 0.0)
			fill.rgb += Aether(inScreen, border.a > 0.0 ? border.rgb : extra.rgb, primitive.Uv.w, primitive.Params.z) * primitive.Uv.z;
		vec4 color = Premultiply(fill);
		if (primitive.Shape.y > 0.0)
			color = mix(Premultiply(border), color, Coverage(d + primitive.Shape.y, pixel));
		if (extra.a > 0.0)
		{
			float glow = exp(-max(-d, 0.0) / max(primitive.Params.y, 1.0));
			color += Premultiply(extra) * glow * (1.0 - color.a);
		}
		result = color * coverage;
		if (primitive.Params.x > 0.0)
		{
			float angle = atan(inLocal.x, -inLocal.y) / TwoPi;
			angle = angle < 0.0 ? angle + 1.0 : angle;
			result *= clamp((angle - primitive.Params.x) * TwoPi * max(length(inLocal), 1.0) / pixel + 0.5, 0.0, 1.0);
		}
		if (primitive.Uv.y > 0.0)
		{
			float band = max(abs(d + 0.5 * primitive.Shape.y) - 0.5 * primitive.Shape.y, 0.0);
			float light = Flow(inLocal, extent, primitive.Uv.w) * exp(-band / 2.5) * smoothstep(7.0, 3.5, band) * primitive.Uv.y;
			result.rgb += mix(border.rgb, vec3(1.0), 0.3) * light * border.a * 0.8;
		}
	}
	else if (mode == ModeGlyph)
	{
		vec4 field = texture(Textures[nonuniformEXT(primitive.Info.y)], inUv);
		vec2 unitRange = vec2(pc.DistanceRange) / vec2(textureSize(Textures[nonuniformEXT(primitive.Info.y)], 0));
		float screenRange = max(0.5 * dot(unitRange, 1.0 / fwidth(inUv)), 1.0);
		float reach = max(0.5 * screenRange - 1.0, 0.5);
		float weight = primitive.Params.x * pc.Scale;
		float outline = min(primitive.Params.y * pc.Scale, reach - weight);
		float softness = min(primitive.Params.z * pc.Scale, reach - weight);
		float distance = (Median(field.r, field.g, field.b) - 0.5) * screenRange + weight;
		float trueDistance = (field.a - 0.5) * screenRange + weight;
		float alpha = softness > 0.0 ? smoothstep(-softness, softness, trueDistance) : clamp(distance + 0.5, 0.0, 1.0);
		if ((primitive.Info.z & FlagShimmer) != 0u)
		{
			float band = fract((inScreen.x + inScreen.y * 0.4) / 900.0 - pc.Time * 0.08);
			float highlight = smoothstep(0.0, 0.05, band) * (1.0 - smoothstep(0.05, 0.14, band));
			fill.rgb = mix(fill.rgb, vec3(1.0), highlight * primitive.Params.w);
		}
		result = Premultiply(fill) * alpha;
		if (outline > 0.0)
			result += Premultiply(border) * clamp(trueDistance + outline + 0.5, 0.0, 1.0) * (1.0 - result.a);
	}
	else if (mode == ModeImage)
	{
		vec4 texel = texture(Textures[nonuniformEXT(primitive.Info.y)], inUv);
		float gray = dot(texel.rgb, vec3(0.3, 0.59, 0.11));
		texel.rgb = mix(vec3(gray), texel.rgb, primitive.Params.x);
		float coverage = primitive.Shape.x > 0.0 ? Coverage(RoundBox(inLocal, extent, primitive.Shape.x), pixel) : 1.0;
		result = texel * Premultiply(fill) * coverage;
	}
	else if (mode == ModeOrb && primitive.Params.w > 0.5)
	{
		float radius = extent.x;
		vec2 q = inLocal / radius;
		float r = length(q);
		float edge = Coverage((r - 1.0) * radius, pixel);
		float amount = clamp(primitive.Shape.x, 0.0, 1.0);
		float time = pc.Time + primitive.Params.y;
		uint style = uint(primitive.Params.w + 0.5);
		float angle = atan(q.y, q.x);
		float fromBottom = acos(clamp(q.y / max(r, 1e-4), -1.0, 1.0));
		vec4 color = vec4(0.0);
		if (style == 1u)
		{
			float rim = pow(smoothstep(0.3, 1.0, r), 1.8);
			float surface = 1.0 - 2.0 * amount - 0.12 * q.x * q.x;
			float soft = mix(0.16, 0.025, smoothstep(0.55, 1.0, r));
			float height = smoothstep(surface - soft, surface + soft, q.y);
			float edge = exp(-pow((r - 0.958) / 0.03, 2.0));
			float vein = RimVeins(angle, r + 0.02, pixel / radius * 1.3, 3.7, time, 0.9);
			float alpha = clamp((0.05 + 0.82 * rim + vein * 0.3 * (0.4 + 0.6 * rim)) * height, 0.0, 0.92);
			vec3 deep = min(bottom.rgb * 3.4, vec3(1.0));
			vec3 tone = deep * (1.0 - 0.25 * rim) * (1.0 - 0.35 * vein);
			float lineAlpha = edge * 0.6 * height;
			vec3 line = mix(deep, top.rgb, 0.4);
			color = vec4(tone * alpha * (1.0 - lineAlpha) + line * lineAlpha, alpha + lineAlpha * (1.0 - alpha));
		}
		else if (style == 2u)
		{
			const float Columns = 22.0;
			float around = (angle + 3.14159265) / 6.2831853 * Columns;
			float fromEdge = 1.0 - r;
			vec2 p = vec2(around, fromEdge * 5.5);
			vec2 cell = floor(p);
			vec2 local = p - cell;
			float first = 8.0;
			float second = 8.0;
			vec2 owner = vec2(0.0);
			vec2 site = vec2(0.0);
			for (int y = -1; y <= 1; y++)
				for (int x = -1; x <= 1; x++)
				{
					vec2 g = vec2(float(x), float(y));
					vec2 c = vec2(mod(cell.x + g.x, Columns), cell.y + g.y);
					vec2 o = vec2(Lattice(c), Lattice(c + 17.3)) * 0.8 + 0.1;
					vec2 d = g + o - local;
					float dd = dot(d * vec2(1.0, 0.45), d * vec2(1.0, 0.45));
					if (dd < first)
					{
						second = first;
						first = dd;
						owner = c;
						site = vec2(cell.x + g.x + o.x, cell.y + g.y + o.y);
					}
					else if (dd < second)
						second = dd;
				}
			float border = sqrt(second) - sqrt(first);
			float depth = site.y / 5.5;
			float reach = 0.3 * amount * (0.55 + 0.9 * Lattice(owner + 5.0));
			float grown = 1.0 - smoothstep(reach - 0.02, reach + 0.01, depth);
			float fade = 1.0 - smoothstep(0.0, reach + 0.05, fromEdge);
			float tone = Lattice(owner + 9.1);
			float shade = 0.3 + 0.7 * tone;
			float crack = (1.0 - smoothstep(0.02, 0.075, border)) * grown;
			float sparkle = pow(0.5 + 0.5 * sin(time * 1.1 + tone * 40.0), 16.0) * grown * fade;
			float alpha = clamp(grown * (0.03 + 0.16 * fade * fade) * shade + crack * 0.12 * fade, 0.0, 0.4) * amount;
			vec3 tint = mix(bottom.rgb, top.rgb, shade);
			vec3 glow = top.rgb * (crack * (0.16 + 0.34 * fade) + sparkle * 0.45 + grown * fade * fade * 0.05 * tone) * amount;
			color = vec4(tint * alpha + glow, alpha);
		}
		result = color * edge;
	}
	else if (mode == ModeOrb)
	{
		float radius = extent.x;
		vec2 q = inLocal / radius;
		float r = length(q);
		float edge = Coverage((r - 1.0) * radius, pixel);
		float level = 1.0 - 2.0 * clamp(primitive.Params.x, 0.0, 1.0);
		float amplitude = primitive.Params.z * smoothstep(0.0, 0.08, primitive.Params.x) * smoothstep(1.0, 0.92, primitive.Params.x);
		float time = pc.Time + primitive.Params.y;
		float surface = level + amplitude * (sin(q.x * 5.0 + time * 2.1) + 0.5 * sin(q.x * 9.0 - time * 3.3));
		float depth = q.y - surface;
		float liquid = clamp(depth * radius / pixel + 0.5, 0.0, 1.0);
		vec3 liquidColor = mix(top.rgb, bottom.rgb, clamp(depth / max(1.0 - surface, 0.25), 0.0, 1.0));
		liquidColor *= 1.0 - 0.5 * pow(r, 4.0);
		liquidColor += vec3(0.35) * exp(-abs(depth) * radius * 0.3) * liquid;
		float swirl = sin(q.x * 3.0 + q.y * 4.0 + time * 0.7) * sin(q.y * 5.0 - time * 0.9);
		liquidColor *= 1.0 + 0.08 * swirl;
		vec2 flow = q * 2.2 + vec2(0.0, time * 0.12);
		liquidColor += top.rgb * smoothstep(0.62, 0.95, Fbm(flow + vec2(Fbm(flow * 1.3 - time * 0.05), 0.0))) * 0.35 * liquid;
		vec3 glass = border.rgb * (1.0 - 0.35 * r);
		vec4 color = mix(Premultiply(vec4(glass, border.a)), vec4(liquidColor, 1.0), liquid);
		vec2 highlight = (q - vec2(-0.32, -0.48)) / vec2(0.46, 0.26);
		float specular = smoothstep(1.0, 0.0, length(highlight)) * extra.a;
		color.rgb += extra.rgb * specular;
		color.a = max(color.a, specular);
		float rim = smoothstep(0.82, 1.0, r) * 0.45;
		color.rgb *= 1.0 - rim;
		result = color * edge;
	}
	else if (mode == ModeTriangle)
	{
		float d = Triangle(inLocal + vec2(0.0, extent.y), vec2(extent.x, 2.0 * extent.y));
		float softness = primitive.Shape.z;
		float coverage = softness > 0.0 ? 1.0 - smoothstep(-softness, softness, d) : Coverage(d, pixel);
		vec4 color = Premultiply(fill);
		if (primitive.Shape.y > 0.0)
			color = mix(Premultiply(border), color, Coverage(d + primitive.Shape.y, pixel));
		result = color * coverage;
	}

	outColor = Finish(primitive, result);
}
