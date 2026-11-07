const uint MaxLakeLights = 24u;

struct LakeFrame
{
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	vec4 Eye;
	vec4 SunDirection;
	vec4 SunRadiance;
	vec4 SkyRadiance;
	vec4 WaterColor;
	vec4 Screen;
	vec4 Plane;
	vec4 Aurora;
	vec4 LightCount;
	vec4 Beast;
	vec4 Lights[48];
};

const uint MeshUnderGlow = 524288u;
const float SquashDepth = 0.76;
const float TagWater = 0.6;
const float TagWall = 0.65;
const float TagIce = 0.7;

float CausticWeb(sampler2D map, vec2 p, float time)
{
	vec2 bent = p + vec2(sin(p.y * 0.31 + sin(p.x * 0.17) * 1.7) + 0.5 * sin(p.y * 0.83 + p.x * 0.29), sin(p.x * 0.27 + sin(p.y * 0.13) * 1.9) + 0.5 * sin(p.x * 0.71 - p.y * 0.37)) * 1.3;
	vec2 first = vec2(bent.x * 0.8 - bent.y * 0.6, bent.x * 0.6 + bent.y * 0.8);
	vec2 second = vec2(bent.x * 0.34 + bent.y * 0.94, bent.y * 0.34 - bent.x * 0.94);
	float a = textureLod(map, first * 0.0937 + vec2(0.019, 0.011) * time, 0.0).r;
	float b = textureLod(map, second * 0.1391 + vec2(-0.014, 0.018) * time + 0.37, 0.0).r;
	float fade = 0.32 + 0.68 * smoothstep(-0.7, 0.8, sin(p.x * 0.123 + sin(p.y * 0.21) * 2.1) + sin(p.y * 0.157 + 1.3));
	return (exp(-a * 11.0) + exp(-b * 13.0)) * fade;
}

float AuroraRibbon(float b, float centre, float below, float above)
{
	float d = b - centre;
	return d < 0.0 ? exp(-d * d / (below * below)) : exp(-d / above);
}

vec3 AuroraSky(sampler2D noise, vec3 position, vec3 direction, float time, float strength)
{
	float up = max(direction.y, 0.12);
	vec2 p = position.xz * 0.012 + direction.xz / up;
	float slow = time * 0.018;
	vec3 total = vec3(0.0);
	for (int i = 0; i < 3; i++)
	{
		float k = float(i);
		float wander = textureLod(noise, vec2(p.x * 0.09 + slow * (0.5 + 0.3 * k) + k * 0.31, 0.17 + k * 0.23), 0.0).r - 0.5;
		float drift = textureLod(noise, vec2(p.x * 0.021 + k * 0.71 - slow * 0.2, 0.41 + k * 0.19), 0.0).r - 0.5;
		float presence = smoothstep(0.22, 0.68, textureLod(noise, vec2(p.x * 0.037 + k * 0.37 + slow * 0.3, 0.9 - k * 0.13), 0.0).r);
		float centre = 0.12 + 0.42 * k + wander * 1.5 + drift * 2.6;
		float ribbon = AuroraRibbon(p.y, centre, 0.05 + 0.02 * k, 0.26 + 0.1 * k);
		float rays = textureLod(noise, vec2(p.x * (0.72 + 0.17 * k) + slow * 2.4 + k * 0.53, 0.61 + k * 0.11 + p.y * 0.012), 0.0).r;
		float fine = textureLod(noise, vec2(p.x * 1.7 - slow * 4.0 + k * 0.17, 0.83 + p.y * 0.015), 0.0).r;
		float curtain = smoothstep(0.3, 0.72, rays) * (0.45 + 0.75 * smoothstep(0.32, 0.78, fine));
		float breath = textureLod(noise, vec2(p.x * 0.05 + k * 0.7 + time * 0.0031, p.y * 0.043 + time * 0.012 + k), 0.0).r;
		float height = clamp((p.y - centre) / 0.55, 0.0, 1.0);
		vec3 tone = mix(vec3(0.06, 1.0, 0.46), vec3(0.42, 0.14, 1.0), smoothstep(0.1, 0.75, height));
		total += tone * (ribbon * curtain * presence * smoothstep(0.32, 0.62, breath + 0.1));
	}
	vec3 dusk = mix(vec3(0.012, 0.03, 0.075), vec3(0.03, 0.06, 0.14), clamp(1.0 - up, 0.0, 1.0));
	return dusk + total * strength;
}
