#version 450
#include "CosmosNoise.glsl"

layout(push_constant) uniform Pass
{
	uint Kind;
	uint Count;
	uint Band;
	uint Bands;
} pass;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

const int LightCount = 5;
const vec3 LightDirections[LightCount] = vec3[](vec3(0.72, 0.28, 0.63), vec3(-0.62, -0.18, 0.76), vec3(-0.34, 0.5, -0.8), vec3(-0.8, -0.3, -0.5), vec3(0.55, -0.45, -0.7));
const vec3 LightColors[LightCount] = vec3[](vec3(1.0, 0.5, 0.72), vec3(0.42, 0.86, 1.0), vec3(1.0, 0.72, 0.42), vec3(0.62, 0.5, 1.0), vec3(0.35, 0.95, 0.85));

vec3 Palette(float hue)
{
	vec3 rose = vec3(0.95, 0.2, 0.42);
	vec3 violet = vec3(0.42, 0.2, 0.95);
	vec3 teal = vec3(0.08, 0.68, 0.86);
	vec3 gold = vec3(1.0, 0.55, 0.22);
	vec3 color = mix(rose, violet, smoothstep(0.15, 0.4, hue));
	color = mix(color, teal, smoothstep(0.42, 0.68, hue));
	return mix(color, gold, smoothstep(0.78, 0.95, hue));
}

vec3 Nebula(vec3 d)
{
	vec3 plane = normalize(vec3(0.3, 1.0, -0.4));
	float latitude = dot(d, plane);
	float milky = exp(-latitude * latitude * 9.0);
	vec3 color = vec3(0.0018, 0.0016, 0.0042) + vec3(0.006, 0.0058, 0.012) * milky;
	float region = Fbm3(d * 1.5 + 4.1, 4);
	float mask = smoothstep(0.36, 0.62, region) + milky * 0.45;
	vec3 lights[LightCount];
	for (int k = 0; k < LightCount; k++)
		lights[k] = normalize(LightDirections[k]) * 1.75;

	const int Steps = 40;
	const float Depth = 1.6;
	const float Step = Depth / float(Steps);
	float transmittance = 1.0;
	for (int i = 0; i < Steps; i++)
	{
		vec3 p = d * (1.0 + (float(i) + 0.5) * Step);
		vec3 q = p * 2.1;
		vec3 warp = vec3(Fbm3(q + 1.7, 3), Fbm3(q + 9.2, 3), Fbm3(q + 4.4, 3)) - 0.5;
		float n = Fbm3(q * 1.3 + warp * 2.8, 5);
		float gas = pow(smoothstep(0.44, 0.86, n), 1.7) * mask;
		float strand = 1.0 - abs(Fbm3(q * 2.4 + warp * 3.4 + 11.0, 5) * 2.0 - 1.0);
		float filament = pow(strand, 14.0) * mask * 1.4;
		float lanes = 1.0 - abs(Fbm3(q * 1.7 - warp * 2.0 + 31.0, 4) * 2.0 - 1.0);
		float dust = (pow(lanes, 9.0) * 3.0 + pow(strand, 5.0) * 0.6) * smoothstep(0.3, 0.6, region);
		vec3 light = vec3(0.03, 0.028, 0.05);
		vec3 tint = vec3(0.0);
		for (int k = 0; k < LightCount; k++)
		{
			vec3 offset = p - lights[k];
			float falloff = 0.14 / (0.025 + dot(offset, offset));
			light += LightColors[k] * falloff;
			tint += LightColors[k] * falloff * falloff;
		}
		float hue = clamp((Fbm3(q * 0.5 + 20.0, 3) - 0.5) * 2.6 + 0.5, 0.0, 1.0);
		vec3 emission = Palette(hue);
		emission = mix(emission, normalize(tint + 1e-4) * 1.7, clamp(dot(tint, vec3(0.33)) * 0.08, 0.0, 0.6));
		color += transmittance * (gas + filament) * emission * light * Step * 1.6;
		transmittance *= exp(-(gas * 0.9 + dust * 2.2) * Step * 3.2);
	}
	for (int k = 0; k < LightCount; k++)
	{
		float angle = 1.0 - dot(d, normalize(LightDirections[k]));
		color += LightColors[k] * (exp(-angle * 40000.0) * 4.0 + exp(-angle * 3000.0) * 0.25 + exp(-angle * 120.0) * 0.025) * max(transmittance, 0.25);
	}
	return color;
}

void main()
{
	vec3 d = OctDecode(inUv);
	vec3 color = Nebula(d);
	float tone = Fbm3(d * 1.3 + 11.0, 2) * 0.6 + Fbm3(d * 2.1 + 37.0, 2) * 0.4;
	outColor = vec4(pass.Bands == 0u ? vec3(0.0) : color, tone);
}
