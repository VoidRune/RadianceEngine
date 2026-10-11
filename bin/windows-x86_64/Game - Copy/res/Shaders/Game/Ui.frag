#version 450
#extension GL_EXT_nonuniform_qualifier : require

struct Primitive
{
	vec4 Rect;
	vec4 Uv;
	vec4 Clip;
	vec4 Shape;
	vec4 Params;
	uvec4 Colors;
	uvec4 Info;
};

layout(std430, set = 0, binding = 0) readonly buffer Primitives { Primitive primitives[]; };
layout(set = 0, binding = 1) uniform sampler2D Textures[8];

layout(push_constant) uniform Constants
{
	vec2 ScreenSize;
	float Scale;
	float DistanceRange;
	float Time;
} pc;

layout(location = 0) in vec2 inLocal;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec2 inScreen;
layout(location = 3) flat in uint inIndex;
layout(location = 0) out vec4 outColor;

const uint ModeRect = 0u;
const uint ModeGlyph = 1u;
const uint ModeImage = 2u;
const uint ModeOrb = 3u;
const uint ModeTriangle = 4u;
const float TwoPi = 6.28318531;

vec4 Premultiply(vec4 color)
{
	return vec4(color.rgb * color.a, color.a);
}

float Median(float a, float b, float c)
{
	return max(min(a, b), min(max(a, b), c));
}

float RoundBox(vec2 p, vec2 extent, float radius)
{
	radius = min(radius, min(extent.x, extent.y));
	vec2 q = abs(p) - extent + radius;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
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

float Coverage(float distance, float pixel)
{
	return clamp(0.5 - distance / pixel, 0.0, 1.0);
}

void main()
{
	Primitive primitive = primitives[inIndex];
	bool clipped = any(lessThan(inScreen, primitive.Clip.xy)) || any(greaterThanEqual(inScreen, primitive.Clip.zw));

	uint mode = primitive.Info.x;
	float pixel = 1.0 / pc.Scale;
	vec2 extent = primitive.Rect.zw;
	vec4 top = unpackUnorm4x8(primitive.Colors.x);
	vec4 bottom = unpackUnorm4x8(primitive.Colors.y);
	vec4 border = unpackUnorm4x8(primitive.Colors.z);
	vec4 extra = unpackUnorm4x8(primitive.Colors.w);
	float along = (primitive.Info.z & 1u) != 0u ? inLocal.x / max(extent.x, 1e-4) : inLocal.y / max(extent.y, 1e-4);
	vec4 fill = mix(top, bottom, clamp(along * 0.5 + 0.5, 0.0, 1.0));
	vec4 result = vec4(0.0);

	if (mode == ModeRect)
	{
		float d = RoundBox(inLocal, extent, primitive.Shape.x);
		float softness = primitive.Shape.z;
		float coverage = softness > 0.0 ? 1.0 - smoothstep(-softness, softness, d) : Coverage(d, pixel);
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

	outColor = clipped || result.a <= 0.002 ? vec4(0.0) : vec4(result.rgb / result.a, result.a);
}
