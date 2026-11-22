#include "HeartGlyphs.glsl"

const float HeartGlyphScale = 1.08;

struct HeartRuneLook
{
	vec3 Albedo;
	float Coverage;
	vec3 Emission;
};

vec2 HeartRuneField(vec2 p, int glyph, float progress)
{
	ivec2 range = HeartGlyphRange[glyph];
	float best = 1e3;
	float fill = -1.0;
	for (int i = range.x; i < range.x + range.y; i++)
	{
		vec4 arc = HeartStrokeArc[i];
		vec4 look = HeartStrokeLook[i];
		vec2 q = p - arc.xy;
		if (abs(length(q) - arc.z) - look.y >= best)
			continue;
		float d;
		float s = 0.5;
		float width = look.y;
		if (arc.z <= 0.0)
			d = length(q);
		else
		{
			float phi = atan(q.y, q.x);
			float t = mod(look.x >= 0.0 ? phi - arc.w : arc.w - phi, Tau);
			float span = abs(look.x);
			if (t <= span)
			{
				d = abs(length(q) - arc.z);
				s = t / span;
			}
			else
			{
				vec4 ends = HeartStrokeEnds[i];
				float da = length(p - ends.xy);
				float db = length(p - ends.zw);
				d = min(da, db);
				s = da < db ? 0.0 : 1.0;
			}
			width *= 0.3 + 0.7 * sqrt(sin(3.14159265 * s));
		}
		float edge = d - width;
		if (edge < best)
		{
			best = edge;
			fill = progress - mix(look.z, look.w, s);
		}
	}
	return vec2(best, fill);
}

vec3 HeartRamp(float t)
{
	vec3 c = mix(vec3(0.35, 0.03, 0.005), vec3(1.0, 0.24, 0.03), smoothstep(0.0, 0.35, t));
	c = mix(c, vec3(1.0, 0.52, 0.1), smoothstep(0.3, 0.65, t));
	return mix(c, vec3(1.0, 0.86, 0.55), smoothstep(0.65, 1.05, t));
}

HeartRuneLook HeartRuneShape(vec2 uv, float pixel, vec4 params, uint seed, float time)
{
	int code = int(params.w + 0.5);
	vec2 p = uv * HeartGlyphScale;
	if (code >= HeartGlyphCount)
		p.x = -p.x;
	float progress = params.x;
	float cooling = params.y;
	float pulse = params.z;
	vec2 field = HeartRuneField(p, code % HeartGlyphCount, progress);
	float edge = field.x;
	float aa = max(pixel * HeartGlyphScale, 0.004);
	float groove = 1.0 - smoothstep(-aa, aa, edge);
	float lip = (1.0 - groove) * (1.0 - smoothstep(0.0, 0.035 + aa, edge));
	float core = smoothstep(0.0, -0.045, edge);
	vec2 offset = vec2(float(seed & 255u), float((seed >> 8) & 255u)) * 0.37;
	float written = progress > 0.0 ? smoothstep(-0.015, 0.0, field.y) : 0.0;
	float age = max(field.y, 0.0);
	float front = written * exp(-age * age / 0.0015) * (1.0 - smoothstep(1.0, 1.08, progress));
	float heat = written * (1.0 - smoothstep(0.0, 1.0, cooling));
	float churn = 0.7 + 0.6 * ValueNoise(p * 9.0 + offset + vec2(time * 0.7, -time * 0.45));
	float temp = clamp(0.22 + 0.5 * heat + 0.3 * written * exp(-age * 5.0) + 0.55 * front, 0.0, 1.2);
	float lit = heat + 0.6 * front;
	vec3 emission = HeartRamp(temp) * churn * groove * (0.45 + 0.55 * core) * (2.4 * lit);
	emission += HeartRamp(0.45) * exp(-max(edge, 0.0) / 0.07) * (1.0 - groove) * (0.55 * lit);
	float embers = written * smoothstep(0.7, 1.3, cooling) * (1.0 - smoothstep(1.3, 2.0, cooling)) * smoothstep(0.55, 0.85, ValueNoise(p * 14.0 + offset * 1.7));
	emission += vec3(0.9, 0.16, 0.025) * (embers * groove * 0.6);
	emission += vec3(0.85, 0.15, 0.025) * pulse * (groove * (0.3 + 0.7 * core) * 0.7 + exp(-max(edge, 0.0) / 0.12) * (1.0 - groove) * 0.12);
	HeartRuneLook look;
	look.Albedo = mix(vec3(0.022, 0.018, 0.016), vec3(0.2, 0.185, 0.17), lip / max(groove + lip, 1e-4));
	look.Coverage = groove * 0.85 + lip * 0.3;
	look.Emission = emission;
	return look;
}
