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

const float TwoPi = 6.28318531;

vec4 Premultiply(vec4 color)
{
	return vec4(color.rgb * color.a, color.a);
}

uint HashU(uint x)
{
	x ^= x >> 16;
	x *= 0x7FEB352Du;
	x ^= x >> 15;
	x *= 0x846CA68Bu;
	x ^= x >> 16;
	return x;
}

float Coverage(float distance, float pixel)
{
	return clamp(0.5 - distance / pixel, 0.0, 1.0);
}

bool Clipped(Primitive primitive)
{
	return any(lessThan(inScreen, primitive.Clip.xy)) || any(greaterThanEqual(inScreen, primitive.Clip.zw));
}

vec4 Finish(Primitive primitive, vec4 result)
{
	result.a *= 1.0 - float(primitive.Info.w & 255u) / 255.0;
	return Clipped(primitive) ? vec4(0.0) : result;
}
