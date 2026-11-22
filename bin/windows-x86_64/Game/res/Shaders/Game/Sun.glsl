struct SunQuad
{
	vec4 CenterSize;
	vec4 Axis;
	vec4 A;
	vec4 B;
	vec4 C;
	vec4 D;
	vec4 E;
	uvec4 Info;
};

struct SunVertex
{
	vec4 PositionWidth;
	vec4 Body;
	vec4 Glow;
	vec4 Tangent;
	vec4 Extra;
};

layout(push_constant) uniform SunConstants
{
	mat4 ViewProjection;
	vec4 Eye;
	vec4 Camera;
	vec4 Screen;
	vec4 Forward;
} pc;

const uint SunStarQuad = 0u;
const uint SunHazeQuad = 1u;
const uint SunWaveQuad = 2u;
const uint SunWardQuad = 3u;

const uint SunPlasma = 0u;
const uint SunSpark = 1u;

const float SunPi = 3.14159265;
const float SunTau = 6.28318531;

float SunLinearDepth(float z)
{
	float near = pc.Camera.x;
	float far = pc.Camera.y;
	return near * far / (far - z * (far - near));
}

float SunHash(vec3 p)
{
	p = fract(p * 0.3183099 + vec3(0.71, 0.113, 0.419));
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

vec3 SunHash3(vec3 p)
{
	return vec3(SunHash(p), SunHash(p + vec3(31.41, 17.73, 9.27)), SunHash(p + vec3(5.93, 47.21, 23.63)));
}

float SunNoise(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	float a = SunHash(i);
	float b = SunHash(i + vec3(1.0, 0.0, 0.0));
	float c = SunHash(i + vec3(0.0, 1.0, 0.0));
	float d = SunHash(i + vec3(1.0, 1.0, 0.0));
	float e = SunHash(i + vec3(0.0, 0.0, 1.0));
	float g = SunHash(i + vec3(1.0, 0.0, 1.0));
	float h = SunHash(i + vec3(0.0, 1.0, 1.0));
	float k = SunHash(i + vec3(1.0, 1.0, 1.0));
	return mix(mix(mix(a, b, f.x), mix(c, d, f.x), f.y), mix(mix(e, g, f.x), mix(h, k, f.x), f.y), f.z);
}

float SunFbm(vec3 p, int octaves)
{
	float total = 0.0;
	float amplitude = 0.5;
	float weight = 0.0;
	for (int i = 0; i < octaves; i++)
	{
		total += amplitude * SunNoise(p);
		weight += amplitude;
		p = p * 2.03 + vec3(1.7, 9.2, 4.1);
		amplitude *= 0.5;
	}
	return total / weight;
}

vec3 SunCells(vec3 p, float time)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	float f1 = 8.0;
	float f2 = 8.0;
	vec3 nearest = vec3(0.0);
	for (int z = -1; z <= 1; z++)
		for (int y = -1; y <= 1; y++)
			for (int x = -1; x <= 1; x++)
			{
				vec3 cell = vec3(float(x), float(y), float(z));
				vec3 h = SunHash3(i + cell);
				vec3 point = cell + 0.5 + 0.4 * sin(time * (0.6 + 0.8 * h) + SunTau * h);
				vec3 d = point - f;
				float distance = dot(d, d);
				if (distance < f1)
				{
					f2 = f1;
					f1 = distance;
					nearest = cell;
				}
				else if (distance < f2)
					f2 = distance;
			}
	return vec3(sqrt(f1), sqrt(f2), SunHash(i + nearest + vec3(13.1, 7.7, 3.3)));
}

vec3 SunRamp(float t)
{
	t = clamp(t, 0.0, 1.25);
	vec3 c = mix(vec3(0.3, 0.035, 0.004), vec3(0.92, 0.22, 0.025), smoothstep(0.0, 0.3, t));
	c = mix(c, vec3(1.0, 0.5, 0.08), smoothstep(0.25, 0.55, t));
	c = mix(c, vec3(1.0, 0.76, 0.34), smoothstep(0.5, 0.8, t));
	c = mix(c, vec3(1.0, 0.93, 0.76), smoothstep(0.78, 1.05, t));
	return c;
}

mat3 SunSpin(float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	return mat3(c, 0.0, -s, 0.0, 1.0, 0.0, s, 0.0, c);
}
