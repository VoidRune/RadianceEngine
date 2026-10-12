#version 450

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

layout(location = 0) out vec2 outLocal;
layout(location = 1) out vec2 outUv;
layout(location = 2) out vec2 outScreen;
layout(location = 3) flat out uint outIndex;

const vec2 Corners[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0));
const uint ModeRect = 0u;
const uint ModeTriangle = 4u;
const uint ModeSigil = 5u;

void main()
{
	Primitive primitive = primitives[gl_InstanceIndex];
	vec2 corner = Corners[gl_VertexIndex];
	uint mode = primitive.Info.x;
	float padding = 0.0;
	if (mode == ModeRect || mode == ModeTriangle)
		padding = primitive.Shape.z + 2.0 / pc.Scale + (mode == ModeRect && primitive.Uv.y > 0.0 ? 7.0 : 0.0);
	else if (mode == ModeSigil)
		padding = 2.0 / pc.Scale;
	vec2 local = corner * (primitive.Rect.zw + padding);
	float s = sin(primitive.Shape.w);
	float c = cos(primitive.Shape.w);
	vec2 screen = primitive.Rect.xy + vec2(local.x * c - local.y * s, local.x * s + local.y * c);

	outLocal = local;
	outUv = mix(primitive.Uv.xy, primitive.Uv.zw, corner * 0.5 + 0.5);
	outScreen = screen;
	outIndex = gl_InstanceIndex;
	gl_Position = vec4(screen / pc.ScreenSize * 2.0 - 1.0, 0.0, 1.0);
}
