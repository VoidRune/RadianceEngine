#version 450

layout(set = 0, binding = 0) uniform sampler2D Scene;
layout(set = 0, binding = 1) uniform sampler2D Bloom;
layout(set = 0, binding = 2) uniform sampler3D Grading;

layout(push_constant) uniform PushConstants
{
	float BloomStrength;
	float Exposure;
	uint Tonemapper;
	uint Flags;
	vec4 Crop;
} pc;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

const uint TonemapAgX = 0u;
const uint TonemapAces = 1u;
const uint TonemapNeutral = 2u;
const uint TonemapSoftClip = 3u;
const uint FlagGrading = 1u;
const uint FlagRaw = 2u;

vec3 LinearToSrgb(vec3 color)
{
	return mix(color * 12.92, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), color));
}

vec3 AgX(vec3 color)
{
	const mat3 inset = mat3(
		0.842479062253094, 0.0423282422610123, 0.0423756549057051,
		0.0784335999999992, 0.878468636469772, 0.0784336,
		0.0792237451477643, 0.0791661274605434, 0.879142973793104);
	const mat3 outset = mat3(
		1.19687900512017, -0.0528968517574562, -0.0529716355144438,
		-0.0980208811401368, 1.15190312990417, -0.0980434501171241,
		-0.0990297440797205, -0.0989611768448433, 1.15107367264116);
	const float minEv = -12.47393;
	const float maxEv = 4.026069;
	vec3 x = clamp((log2(max(inset * color, vec3(1e-10))) - minEv) / (maxEv - minEv), 0.0, 1.0);
	vec3 x2 = x * x;
	vec3 x4 = x2 * x2;
	x = 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
	return clamp(outset * x, 0.0, 1.0);
}

vec3 Neutral(vec3 color)
{
	const float startCompression = 0.76;
	const float desaturation = 0.15;
	float low = min(color.r, min(color.g, color.b));
	color -= low < 0.08 ? low - 6.25 * low * low : 0.04;
	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression)
		return color;
	const float d = 1.0 - startCompression;
	float newPeak = 1.0 - d * d / (peak + d - startCompression);
	color *= newPeak / peak;
	float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
	return mix(color, vec3(newPeak), g);
}

vec3 SoftClip(vec3 color)
{
	const float shoulder = 0.9;
	const float room = 1.0 - shoulder;
	return min(color, vec3(shoulder)) + room * (1.0 - exp(-max(color - shoulder, 0.0) / room));
}

vec3 Aces(vec3 color)
{
	const mat3 inputMatrix = mat3(0.59719, 0.07600, 0.02840, 0.35458, 0.90834, 0.13383, 0.04823, 0.01566, 0.83777);
	const mat3 outputMatrix = mat3(1.60475, -0.10208, -0.00327, -0.53108, 1.10813, -0.07276, -0.07367, -0.00605, 1.07602);
	color = inputMatrix * color;
	vec3 a = color * (color + 0.0245786) - 0.000090537;
	vec3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
	return clamp(outputMatrix * (a / b), 0.0, 1.0);
}

void main()
{
	vec2 uv = inUv * pc.Crop.xy + pc.Crop.zw;
	vec3 color = max(textureLod(Scene, uv, 0.0).rgb, vec3(0.0));
	if ((pc.Flags & FlagRaw) != 0u)
	{
		outColor = vec4(clamp(color, 0.0, 1.0), 1.0);
		return;
	}
	color = (color + textureLod(Bloom, uv, 0.0).rgb * pc.BloomStrength) * pc.Exposure;
	vec3 display;
	if (pc.Tonemapper == TonemapAgX)
		display = AgX(color);
	else if (pc.Tonemapper == TonemapAces)
		display = LinearToSrgb(Aces(color));
	else if (pc.Tonemapper == TonemapNeutral)
		display = LinearToSrgb(Neutral(color));
	else if (pc.Tonemapper == TonemapSoftClip)
		display = pow(SoftClip(color), vec3(1.0 / 2.2));
	else
		display = LinearToSrgb(clamp(color, 0.0, 1.0));
	if ((pc.Flags & FlagGrading) != 0u)
	{
		float size = float(textureSize(Grading, 0).x);
		display = textureLod(Grading, display * ((size - 1.0) / size) + 0.5 / size, 0.0).rgb;
	}
	outColor = vec4(display, 1.0);
}
