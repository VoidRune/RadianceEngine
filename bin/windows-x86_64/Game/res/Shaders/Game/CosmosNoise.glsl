const float Pi = 3.14159265;
const float TwoPi = 6.28318531;

float Hash13(vec3 p3)
{
	p3 = fract(p3 * 0.1031);
	p3 += dot(p3, p3.zyx + 31.32);
	return fract((p3.x + p3.y) * p3.z);
}

vec4 Hash43(vec3 p)
{
	vec4 p4 = fract(vec4(p.xyzx) * vec4(0.1031, 0.1030, 0.0973, 0.1099));
	p4 += dot(p4, p4.wzxy + 33.33);
	return fract((p4.xxyz + p4.yzzw) * p4.zywx);
}

float Noise3(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = Hash13(i);
	float b = Hash13(i + vec3(1.0, 0.0, 0.0));
	float c = Hash13(i + vec3(0.0, 1.0, 0.0));
	float d = Hash13(i + vec3(1.0, 1.0, 0.0));
	float e = Hash13(i + vec3(0.0, 0.0, 1.0));
	float g = Hash13(i + vec3(1.0, 0.0, 1.0));
	float h = Hash13(i + vec3(0.0, 1.0, 1.0));
	float k = Hash13(i + vec3(1.0, 1.0, 1.0));
	return mix(mix(mix(a, b, f.x), mix(c, d, f.x), f.y), mix(mix(e, g, f.x), mix(h, k, f.x), f.y), f.z);
}

float Fbm3(vec3 p, int octaves)
{
	float value = 0.0;
	float amplitude = 0.5;
	float total = 0.0;
	for (int i = 0; i < octaves; i++)
	{
		value += amplitude * Noise3(p);
		total += amplitude;
		p = p * 2.03 + vec3(17.1, 9.7, 3.3);
		amplitude *= 0.5;
	}
	return value / total;
}

vec2 OctEncode(vec3 n)
{
	n /= abs(n.x) + abs(n.y) + abs(n.z);
	vec2 p = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * vec2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
	return p * 0.5 + 0.5;
}

vec3 OctDecode(vec2 uv)
{
	vec2 f = uv * 2.0 - 1.0;
	vec3 n = vec3(f, 1.0 - abs(f.x) - abs(f.y));
	float t = max(-n.z, 0.0);
	n.x += n.x >= 0.0 ? -t : t;
	n.y += n.y >= 0.0 ? -t : t;
	return normalize(n);
}
