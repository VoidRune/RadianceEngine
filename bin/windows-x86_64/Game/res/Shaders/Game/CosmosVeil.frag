#version 450

layout(push_constant) uniform Pass
{
	float Time;
	float Aspect;
	float Quality;
	float Padding;
} pass;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

float Hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float Noise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(Hash12(i), Hash12(i + vec2(1.0, 0.0)), f.x), mix(Hash12(i + vec2(0.0, 1.0)), Hash12(i + vec2(1.0, 1.0)), f.x), f.y);
}

float Fbm(vec2 p, int octaves)
{
	float value = 0.0;
	float amplitude = 0.5;
	float total = 0.0;
	for (int i = 0; i < octaves; i++)
	{
		value += amplitude * Noise(p);
		total += amplitude;
		p = mat2(1.6, 1.2, -1.2, 1.6) * p + vec2(7.3, 3.1);
		amplitude *= 0.5;
	}
	return value / total;
}

void main()
{
	float time = pass.Time;
	vec2 p = (inUv - 0.5) * vec2(pass.Aspect, 1.0);
	float r = length(p);
	float angle = atan(p.y, p.x) + 0.55 / (0.3 + r) + time * 0.02;
	vec2 q = vec2(cos(angle), sin(angle)) * r;
	vec2 warp = vec2(Fbm(q * 2.1 + vec2(time * 0.017, 0.0), 3), Fbm(q * 2.1 + vec2(5.2, -time * 0.013), 3));
	float cloud = Fbm(q * 2.8 + warp * 1.9, 5);
	float gas = smoothstep(0.4, 0.9, cloud);
	float haze = Fbm(q * 0.9 + warp * 0.6 + 40.0, 3);
	float strands = pow(1.0 - abs(Fbm(q * 3.4 + warp * 2.2 + 9.0, 4) * 2.0 - 1.0), 4.0) * gas;
	float hue = clamp((Fbm(q * 1.1 + vec2(30.0, time * 0.008), 3) - 0.5) * 2.8 + 0.5, 0.0, 1.0);
	vec3 tint = mix(vec3(0.95, 0.3, 0.5), vec3(0.42, 0.3, 0.92), smoothstep(0.2, 0.45, hue));
	tint = mix(tint, vec3(0.12, 0.62, 0.85), smoothstep(0.55, 0.8, hue));
	tint = mix(tint, vec3(1.0, 0.68, 0.4), smoothstep(0.9, 1.0, hue) * 0.6);
	float counter = atan(p.y, p.x) - 0.35 / (0.4 + r) - time * 0.013;
	vec2 q2 = vec2(cos(counter), sin(counter)) * r;
	float second = smoothstep(0.38, 0.85, Fbm(q2 * 1.9 + warp.yx * 1.4 + 70.0, 4));
	float hue2 = clamp((Fbm(q2 * 0.8 + 55.0, 2) - 0.5) * 2.6 + 0.5, 0.0, 1.0);
	vec3 tint2 = mix(vec3(0.2, 0.55, 0.9), vec3(0.75, 0.3, 0.75), hue2);
	float calm = smoothstep(0.12, 0.7, r);
	vec3 color = tint * (gas * 0.45 + strands * 0.38 + haze * haze * 0.1) * (0.15 + 0.85 * calm);
	color += tint2 * (second * 0.12 + 0.022 + haze * 0.02) * calm * calm;
	color += vec3(0.5, 0.62, 1.0) * exp(-r * r * 9.0) * (0.07 + 0.02 * sin(time * 0.7));
	outColor = vec4(color, calm);
}
