#include "CosmosNoise.glsl"

struct CosmosFrame
{
	mat4 ViewProjection;
	mat4 PreviousViewProjection;
	vec4 Camera;
	vec4 Right;
	vec4 Up;
	vec4 Forward;
	vec4 Screen;
	vec4 Lens;
	vec4 Mouse;
	vec4 Tint;
	vec4 Galaxy;
	vec4 Title;
	vec4 Counts;
	vec4 Cloud;
	vec4 CloudState;
	vec4 Warden[11];
	vec4 WardenBones[21];
};

struct TitleGlyph
{
	vec4 Rect;
	vec4 Uv;
	vec4 Area;
};

layout(std430, set = 0, binding = 0) readonly buffer Frame
{
	CosmosFrame frame;
	TitleGlyph glyphs[];
};

const float GalaxyScale = 0.3;
const float BulgeRadius = 0.16;
const float BulgeFlattening = 0.8;

float Erf(float x)
{
	float x2 = x * x;
	float a = 0.147;
	float value = sqrt(1.0 - exp(-x2 * (1.2732395 + a * x2) / (1.0 + a * x2)));
	return x < 0.0 ? -value : value;
}

float StarHeight(float r)
{
	return 0.03 + 0.05 * r;
}

float DustHeight(float r)
{
	return 0.01 + 0.016 * r;
}

float YoungHeight(float r)
{
	return 0.012 + 0.014 * r;
}

const float ArmPitch = 1.95;
const float ArmStart = 0.16;

float ArmAngle(float r, float arm)
{
	float wiggle = 0.16 * sin(r * 13.0 + arm * 1.7) + 0.07 * sin(r * 31.0 + arm * 4.1);
	return log(max(r, 0.02) / ArmStart) * ArmPitch + wiggle * smoothstep(0.15, 0.5, r) + arm * Pi;
}

float ArmWidth(float r)
{
	return 0.06 + 0.06 * r;
}

float WrapAngle(float x)
{
	return x - TwoPi * floor(x / TwoPi + 0.5);
}

float ArmDensity(float r, float phi, float pattern)
{
	float width = ArmWidth(r) / max(r, 0.05);
	float sum = 0.0;
	for (int i = 0; i < 2; i++)
	{
		float delta = WrapAngle(phi + pattern - ArmAngle(r, float(i)));
		sum += exp(-delta * delta / (2.0 * width * width)) + 0.3 * exp(-delta * delta / (11.0 * width * width));
	}
	return sum * smoothstep(0.1, 0.3, r);
}

float Noise2(vec2 p)
{
	return Noise3(vec3(p, 0.5));
}

vec2 GalaxyPolar(float r, float phi)
{
	float swirl = phi + r * 3.0;
	return vec2(cos(swirl), sin(swirl)) * r;
}

float GalaxyDust(float r, float phi, float pattern, float arms)
{
	vec2 polar = GalaxyPolar(r, phi + pattern);
	float lanes = ArmDensity(r * 1.07, phi, pattern);
	float strands = 1.0 - abs(Noise2(polar * 17.0 + 7.0) * 2.0 - 1.0);
	float dust = smoothstep(0.2, 0.8, lanes) * (1.0 - 0.55 * min(arms, 1.0)) * (0.3 + 0.95 * strands * strands);
	dust += 0.35 * smoothstep(0.5, 0.85, Noise2(polar * 7.0 + 20.0)) * strands * strands * strands;
	return dust * smoothstep(0.06, 0.3, r) * smoothstep(1.35, 0.6, r);
}

float LaneDust(float r)
{
	return 0.22 * smoothstep(0.05, 0.3, r) * smoothstep(1.3, 0.6, r);
}

float DiskDust(float r, float phi, float pattern, float arms)
{
	return GalaxyDust(r, phi, pattern, arms) + LaneDust(r);
}

vec3 Tonemap(vec3 color)
{
	color = max(color, 0.0);
	return clamp(color * (2.51 * color + 0.03) / (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);
}

