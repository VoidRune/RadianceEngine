const float DissolveRamp = 0.55;

float DissolveHash(vec3 p)
{
	p = fract(p * 0.3183099 + 0.1) * 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float DissolveNoise(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	float near = mix(mix(DissolveHash(i), DissolveHash(i + vec3(1.0, 0.0, 0.0)), u.x), mix(DissolveHash(i + vec3(0.0, 1.0, 0.0)), DissolveHash(i + vec3(1.0, 1.0, 0.0)), u.x), u.y);
	float far = mix(mix(DissolveHash(i + vec3(0.0, 0.0, 1.0)), DissolveHash(i + vec3(1.0, 0.0, 1.0)), u.x), mix(DissolveHash(i + vec3(0.0, 1.0, 1.0)), DissolveHash(i + vec3(1.0, 1.0, 1.0)), u.x), u.y);
	return mix(near, far, u.z);
}

float DissolveField(vec3 p, float scale, float sweep, float seed)
{
	vec3 q = p * scale + vec3(seed * 17.31, seed * 7.13, seed * 3.71);
	float n = 0.58 * DissolveNoise(q) + 0.29 * DissolveNoise(q * 2.07 + 5.2) + 0.13 * DissolveNoise(q * 4.31 + 1.7);
	n = clamp((n - 0.5) * 1.9 + 0.5, 0.0, 1.0);
	if (sweep != 0.0)
	{
		float ramp = clamp(p.y / abs(sweep), 0.0, 1.0);
		n = mix(n, sweep > 0.0 ? ramp : 1.0 - ramp, DissolveRamp);
	}
	return n;
}

float DissolveThreshold(float amount, float width)
{
	return mix(-width, 1.0, clamp(amount, 0.0, 1.0));
}

float DissolveEdge(float field, float threshold, float width)
{
	return 1.0 - smoothstep(threshold, threshold + width, field);
}
