struct BlastData
{
	vec4 GroundRadius;
	vec4 Look;
	vec4 Shape;
};

layout(push_constant) uniform BlastConstants
{
	mat4 ViewProjection;
	vec4 Eye;
	vec4 Camera;
	vec4 Screen;
} pc;

const uint BlastDomeSegments = 64u;
const uint BlastDomeRings = 18u;
const uint BlastPuffSegments = 16u;
const uint BlastPuffRings = 10u;
const uint BlastPuffVertices = BlastPuffSegments * BlastPuffRings * 6u;
const uint BlastFirePuffs = 15u;
const uint BlastOuterPuffs = 10u;
const uint BlastSmokePuffs = 16u;
const uint BlastJets = 20u;
const uint BlastJetSegments = 16u;
const uint BlastDomeEnd = BlastDomeSegments * BlastDomeRings * 6u;
const uint BlastFireEnd = BlastDomeEnd + BlastFirePuffs * BlastPuffVertices;
const uint BlastJetEnd = BlastFireEnd + BlastJets * BlastJetSegments * 6u;

const float BlastDome = 0.0;
const float BlastPuff = 1.0;
const float BlastJet = 2.0;
const float BlastSmoke = 3.0;

const float BlastPi = 3.14159265;
const float BlastTau = 6.28318531;

float BlastHash(vec2 p)
{
	vec3 q = fract(vec3(p.xyx) * 0.1031);
	q += dot(q, q.yzx + 33.33);
	return fract((q.x + q.y) * q.z);
}

float BlastNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = BlastHash(i);
	float b = BlastHash(i + vec2(1.0, 0.0));
	float c = BlastHash(i + vec2(0.0, 1.0));
	float d = BlastHash(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float BlastLoopNoise(vec2 p, float period)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float x0 = mod(i.x, period);
	float x1 = mod(i.x + 1.0, period);
	float a = BlastHash(vec2(x0, i.y));
	float b = BlastHash(vec2(x1, i.y));
	float c = BlastHash(vec2(x0, i.y + 1.0));
	float d = BlastHash(vec2(x1, i.y + 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float BlastHash3(vec3 p)
{
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}

float BlastNoise3(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = mix(BlastHash3(i), BlastHash3(i + vec3(1.0, 0.0, 0.0)), f.x);
	float b = mix(BlastHash3(i + vec3(0.0, 1.0, 0.0)), BlastHash3(i + vec3(1.0, 1.0, 0.0)), f.x);
	float c = mix(BlastHash3(i + vec3(0.0, 0.0, 1.0)), BlastHash3(i + vec3(1.0, 0.0, 1.0)), f.x);
	float d = mix(BlastHash3(i + vec3(0.0, 1.0, 1.0)), BlastHash3(i + vec3(1.0, 1.0, 1.0)), f.x);
	return mix(mix(a, b, f.y), mix(c, d, f.y), f.z);
}

float BlastEaseOut(float x)
{
	float i = 1.0 - clamp(x, 0.0, 1.0);
	return 1.0 - i * i * i;
}

float BlastLinearDepth(float z)
{
	float near = pc.Camera.x;
	float far = pc.Camera.y;
	return near * far / (far - z * (far - near));
}
