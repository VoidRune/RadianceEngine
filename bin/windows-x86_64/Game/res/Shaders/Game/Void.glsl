struct VoidQuad
{
	vec4 CenterSize;
	vec4 Axis;
	vec4 A;
	vec4 B;
	vec4 C;
	uvec4 Info;
};

struct VoidVertex
{
	vec4 PositionWidth;
	vec4 Body;
	vec4 Glow;
	vec4 Tangent;
	vec4 Extra;
};

layout(push_constant) uniform VoidConstants
{
	mat4 ViewProjection;
	vec4 Eye;
	vec4 Camera;
	vec4 Screen;
	vec4 Right;
} pc;

const uint VoidSingularity = 0u;
const uint VoidDisk = 1u;
const uint VoidShock = 2u;

const uint RibbonTendril = 0u;
const uint RibbonStream = 1u;
const uint RibbonMote = 2u;

const float VoidPi = 3.14159265;
const float VoidTau = 6.28318531;

float VoidHash(vec2 p)
{
	vec3 q = fract(vec3(p.xyx) * 0.1031);
	q += dot(q, q.yzx + 33.33);
	return fract((q.x + q.y) * q.z);
}

float VoidNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = VoidHash(i);
	float b = VoidHash(i + vec2(1.0, 0.0));
	float c = VoidHash(i + vec2(0.0, 1.0));
	float d = VoidHash(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float VoidLoopNoise(vec2 p, float period)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float x0 = mod(i.x, period);
	float x1 = mod(i.x + 1.0, period);
	float a = VoidHash(vec2(x0, i.y));
	float b = VoidHash(vec2(x1, i.y));
	float c = VoidHash(vec2(x0, i.y + 1.0));
	float d = VoidHash(vec2(x1, i.y + 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float VoidFbm(vec2 p)
{
	return 0.55 * VoidNoise(p) + 0.3 * VoidNoise(p * 2.07 + 5.3) + 0.15 * VoidNoise(p * 4.13 + 1.7);
}

float VoidLinearDepth(float z)
{
	float near = pc.Camera.x;
	float far = pc.Camera.y;
	return near * far / (far - z * (far - near));
}
