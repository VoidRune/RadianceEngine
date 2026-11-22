#version 450
#include "Portal.glsl"

layout(set = 0, binding = 1) uniform sampler2D SceneDepth;
layout(set = 0, binding = 2) uniform sampler2D NoiseField;

layout(location = 0) in vec2 inLocal;
layout(location = 1) flat in uint inIndex;
layout(location = 2) in vec3 inWorld;

layout(location = 0) out vec4 outScene;
layout(location = 1) out vec4 outEmission;

const mat2 OctaveTurn = mat2(0.8, -0.6, 0.6, 0.8);
const vec2 ViewReference = vec2(0.0, -1.25);
const vec2 Drift = vec2(0.31, 0.95);

float Hash(vec2 p)
{
	vec3 q = fract(vec3(p.xyx) * 0.1031);
	q += dot(q, q.yzx + 33.33);
	return fract((q.x + q.y) * q.z);
}

float Field(vec2 p)
{
	return texture(NoiseField, p * (1.0 / 18.0)).r;
}

float Fbm(vec2 p, int octaves)
{
	float base = Field(p);
	if (octaves <= 1)
		return base;
	float detail = 0.74 * base + 0.26 * Field(OctaveTurn * p * 2.9 + vec2(5.3, 1.9));
	if (octaves <= 2)
		return detail;
	return 0.86 * detail + 0.14 * Field(OctaveTurn * OctaveTurn * p * 8.3 + vec2(1.7, 9.2));
}

float LinearDepth(float z)
{
	return pc.Camera.x * pc.Camera.y / (pc.Camera.y - z * (pc.Camera.y - pc.Camera.x));
}

vec3 Stars(vec2 q, float density, float radius, float time, float seed)
{
	vec2 cell = floor(q);
	vec2 local = fract(q) - 0.5;
	float pick = Hash(cell + seed);
	if (pick > density)
		return vec3(0.0);
	float pixel = max(fwidth(q.x), fwidth(q.y));
	vec2 spot = (vec2(Hash(cell + seed + 3.1), Hash(cell + seed + 7.7)) - 0.5) * 0.7;
	float magnitude = pow(Hash(cell + seed + 5.9), 5.0);
	float size = max(radius * (0.6 + 0.8 * magnitude), pixel * 1.2);
	float d = length(local - spot);
	float light = smoothstep(size, 0.0, d) * min(radius / size, 1.0) * min(radius / size, 1.0);
	vec2 arm = abs(local - spot);
	float glint = magnitude * smoothstep(0.65, 1.0, magnitude) * (exp(-arm.x * 90.0) * exp(-arm.y * 6.0) + exp(-arm.y * 90.0) * exp(-arm.x * 6.0)) * 0.35;
	float twinkle = 0.75 + 0.25 * sin(time * (1.1 + pick * 7.0) + pick * 71.0);
	float warmth = Hash(cell + seed + 11.3);
	vec3 tint = mix(vec3(0.68, 0.78, 1.0), vec3(1.0, 0.84, 0.66), warmth * warmth);
	return tint * (light + glint) * twinkle * (0.5 + 3.0 * magnitude);
}

vec3 Gas(vec2 q, float seed, int detail, out float density)
{
	vec2 warp = vec2(Fbm(q * 0.55 + vec2(seed, 0.0), 2), Fbm(q * 0.55 + vec2(5.2, seed + 1.3), 2)) - 0.5;
	float gas = Fbm(q * 0.8 + warp * 2.2, detail);
	float glow = Fbm(q * 1.6 - warp * 1.4 + 3.7, 2);
	float band = exp(-pow(dot(q, vec2(0.55, -0.83)) * 0.32, 2.0));
	vec3 indigo = vec3(0.05, 0.05, 0.24);
	vec3 violet = vec3(0.34, 0.11, 0.66);
	vec3 rose = vec3(0.7, 0.18, 0.5);
	vec3 teal = vec3(0.07, 0.4, 0.78);
	vec3 color = mix(indigo, violet, smoothstep(0.38, 0.7, gas));
	color = mix(color, rose, smoothstep(0.64, 0.86, gas) * 0.5);
	color = mix(color, teal, smoothstep(0.56, 0.8, glow) * (1.0 - smoothstep(0.5, 0.78, gas)) * 0.75);
	density = pow(smoothstep(0.3, 0.76, gas), 1.3) * (0.35 + 0.65 * band) * (0.55 + 0.45 * glow);
	return color * density;
}

float Dust(vec2 q, float seed)
{
	vec2 warp = vec2(Fbm(q * 0.7 + seed, 2), Fbm(q * 0.7 + seed + 4.1, 2)) - 0.5;
	float lane = Fbm(q * 1.3 + warp * 2.6, 3);
	return smoothstep(0.5, 0.74, lane);
}

void main()
{
	PortalSheet sheet = Sheets[inIndex];
	float halfWidth = sheet.BaseHalf.w;
	float spring = sheet.RightSpring.w;
	float apex = sheet.NormalApex.w;
	float intensity = sheet.Params.x;
	float seed = sheet.Params.y;
	float surge = sheet.Params.z;
	vec2 p = inLocal;

	float edge = p.y < spring ? abs(p.x) - halfWidth : length(p - vec2(0.0, spring)) - (apex - spring);
	edge = max(edge, -p.y);
	float pixel = max(fwidth(edge), 1e-4);
	float inside = 1.0 - smoothstep(-pixel, pixel, edge);
	if (inside <= 0.0)
		discard;

	vec2 screen = gl_FragCoord.xy / pc.Screen.xy;
	float selfZ = LinearDepth(gl_FragCoord.z);
	float sceneZ = LinearDepth(textureLod(SceneDepth, screen, 0.0).r);
	if (sceneZ < selfZ - 0.008)
		discard;

	float time = pc.Eye.w + seed * 0.37;
	vec3 right = sheet.RightSpring.xyz;
	vec3 normal = sheet.NormalApex.xyz;
	vec3 ray = normalize(inWorld - pc.Eye.xyz);
	float into = max(dot(ray, -normal), 0.25);
	vec2 lean = clamp(vec2(dot(ray, right), ray.y) / into - ViewReference, vec2(-2.2), vec2(2.2));
	vec2 q = p - vec2(0.0, spring * 0.62);
	vec2 flow = Drift * time;

	vec3 color = mix(vec3(0.004, 0.003, 0.02), vec3(0.018, 0.008, 0.045), smoothstep(-2.0, 2.5, q.y + 0.4 * q.x));

	vec2 far = q + lean * 26.0 + flow * 0.004;
	color += Stars(far * 1.6, 0.5, 0.11, time, 1.0) * 0.55;
	color += Stars(far * 3.1, 0.45, 0.09, time, 17.0) * 0.35;

	float farDensity;
	vec3 farGas = Gas((q + lean * 14.0 + flow * 0.01) * 0.22, seed, 2, farDensity);
	color += farGas * 1.5;
	vec2 heart = q + lean * 0.9 + vec2(0.35, 0.6);
	color += vec3(0.42, 0.24, 0.95) * exp(-dot(heart, heart) / 9.0) * 0.32 + vec3(0.85, 0.6, 1.0) * exp(-dot(heart, heart) / 1.6) * 0.18;

	color += Stars((q + lean * 9.0 + flow * 0.016) * 2.4, 0.4, 0.08, time, 31.0) * 0.6;

	float midDensity;
	vec3 midGas = Gas((q + lean * 5.5 + flow * 0.028) * 0.42 + vec2(4.0, 1.7), seed + 3.0, 3, midDensity);
	color = color * (1.0 - 0.35 * midDensity) + midGas * 1.35;

	float dust = Dust((q + lean * 3.2 + flow * 0.04) * 0.6, seed + 7.0);
	color *= 1.0 - 0.72 * dust;

	color += Stars((q + lean * 2.6 + flow * 0.045) * 2.0, 0.22, 0.07, time, 47.0) * 0.8;

	float nearDensity;
	vec3 nearGas = Gas((q + lean * 1.4 + flow * 0.07) * 0.75 + vec2(-2.3, 6.1), seed + 9.0, 3, nearDensity);
	color += nearGas * 0.5 * (1.0 - 0.5 * dust);

	color += Stars((q + lean * 0.6 + flow * 0.1) * 1.1, 0.12, 0.06, time, 59.0) * 1.1;

	float rim = exp(edge / 0.16);
	float shimmer = Fbm(p * 2.2 + vec2(edge * 5.0, time * 0.4), 2);
	color += vec3(0.55, 0.32, 0.95) * rim * (0.35 + 0.65 * shimmer);
	color += vec3(0.9, 0.75, 1.1) * exp(edge / 0.035) * 0.45;

	color *= intensity * (1.0 + 0.4 * surge);
	outScene = vec4(color, 1.0) * inside;
	outEmission = vec4(color * pc.Camera.w, 1.0) * inside;
}
