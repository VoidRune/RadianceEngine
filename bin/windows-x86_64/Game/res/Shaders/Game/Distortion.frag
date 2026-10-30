#version 450
#include "Distortion.glsl"

layout(set = 0, binding = 1) uniform sampler2D SceneDepth;

layout(location = 0) in vec2 inLocal;
layout(location = 1) flat in uint inIndex;

layout(location = 0) out vec4 outOffset;

const float Pi = 3.14159265;
const float MaxOffset = 0.35;

uint Hash(uint x)
{
	x ^= x >> 16;
	x *= 0x7FEB352Du;
	x ^= x >> 15;
	x *= 0x846CA68Bu;
	x ^= x >> 16;
	return x;
}

vec2 Gradient(ivec2 cell, uint seed)
{
	uint h = Hash((uint(cell.x) * 0x8DA6B343u) ^ (uint(cell.y) * 0xD8163841u) ^ seed);
	return vec2(float(h & 0xFFFFu), float(h >> 16)) * (2.0 / 65535.0) - 1.0;
}

float Noise(vec2 p, uint seed)
{
	vec2 cell = floor(p);
	vec2 f = p - cell;
	ivec2 i = ivec2(cell);
	vec2 u = f * f * (3.0 - 2.0 * f);
	float a = dot(Gradient(i, seed), f);
	float b = dot(Gradient(i + ivec2(1, 0), seed), f - vec2(1.0, 0.0));
	float c = dot(Gradient(i + ivec2(0, 1), seed), f - vec2(0.0, 1.0));
	float d = dot(Gradient(i + ivec2(1, 1), seed), f - vec2(1.0, 1.0));
	return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float LinearDepth(float z)
{
	float near = pc.CameraUp.w;
	float far = pc.Screen.z;
	return near * far / (far - z * (far - near));
}

float Wave(float s)
{
	return -s * exp(-2.0 * s * s) * 3.3;
}

void main()
{
	DistorterInstance distorter = Distorters[inIndex];
	vec2 q = inLocal;
	vec2 dx = dFdx(q);
	vec2 dy = dFdy(q);
	float r = length(q);
	vec2 dir = r > 1e-5 ? q / r : vec2(0.0);
	float strength = distorter.Extent.z;
	vec4 p = distorter.Params;
	uint shape = distorter.Info.x;
	vec2 shift = vec2(0.0);
	if (shape == DistortShockwave)
	{
		float arc = 1.0;
		if (p.z < Pi)
			arc = 1.0 - smoothstep(p.z, p.z + max(p.w, 1e-3), abs(atan(q.y, q.x)));
		shift = dir * (strength * Wave((r - p.x) / max(p.y, 1e-3)) * arc);
	}
	else if (shape == DistortHaze)
	{
		float rise = p.y * pc.CameraRight.w;
		uint seed = distorter.Info.z;
		vec2 at = q * p.x;
		vec2 n = vec2(
			Noise(at + vec2(0.0, -rise), seed) + 0.5 * Noise(at * 2.1 + vec2(0.37 * rise, -1.7 * rise), seed + 17u),
			Noise(at + vec2(5.2, 1.3 - rise), seed + 31u) + 0.5 * Noise(at * 2.3 + vec2(-0.41 * rise, 3.1 - 1.9 * rise), seed + 47u));
		float window;
		if (distorter.Info.y == DistortGround)
			window = smoothstep(0.0, p.z, distorter.Extent.x - abs(q.x)) * smoothstep(0.0, p.z, distorter.Extent.y - abs(q.y));
		else
			window = 1.0 - smoothstep(distorter.Extent.x - p.z, distorter.Extent.x, r);
		shift = n * (strength * window);
	}
	else if (shape == DistortImplosion)
	{
		float u = r / max(p.x, 1e-3);
		int count = int(p.z);
		if (count == 0)
		{
			float core = 0.12;
			float well = 2.0 * core * u / (u * u + core * core);
			shift = -dir * (strength * well * (1.0 - smoothstep(0.5, 1.0, u)));
		}
		else
		{
			float pinch = u < 1.0 ? u * (1.0 - u) * (1.0 - u) * 6.75 : 0.0;
			float ripples = 0.0;
			for (int k = 0; k < count; k++)
			{
				float ring = 1.0 - fract(p.y + float(k) / float(count));
				ripples += Wave((u - ring) / 0.09) * smoothstep(1.0, 0.75, ring) * smoothstep(0.0, 0.2, ring);
			}
			shift = dir * (strength * (1.0 - smoothstep(0.85, 1.0, u)) * (0.6 * pinch + 0.5 * ripples));
		}
	}
	else
	{
		float einstein = p.y;
		float reach = distorter.Extent.x;
		float inner = smoothstep(0.75 * p.x, p.x, r);
		float outer = 1.0 - smoothstep(0.5 * reach, reach, r);
		shift = -dir * (strength * einstein * einstein / max(r, 1e-3) * inner * outer);
	}

	mat2 jacobian = mat2(dx, dy);
	vec2 offset = abs(determinant(jacobian)) > 1e-12 ? inverse(jacobian) * shift / pc.Screen.xy : vec2(0.0);
	float span = length(offset);
	if (span > MaxOffset)
		offset *= MaxOffset / span;

	vec2 screen = gl_FragCoord.xy / pc.Screen.xy;
	float self = LinearDepth(gl_FragCoord.z);
	float soft = distorter.Extent.w;
	float visible = clamp((LinearDepth(textureLod(SceneDepth, screen, 0.0).r) - self) / soft, 0.0, 1.0);
	float keep = clamp((LinearDepth(textureLod(SceneDepth, screen + offset, 0.0).r) - self) / soft + 1.0, 0.0, 1.0);
	outOffset = vec4(offset * (visible * keep), 0.0, 0.0);
}
