vec3 FireRamp(vec3 hue, float heat)
{
	float peak = max(max(hue.r, hue.g), max(hue.b, 1e-4));
	vec3 tint = hue / peak;
	float h = clamp(heat, 0.0, 1.0);
	float cool = smoothstep(0.3, 1.0, tint.b);
	vec3 tone = pow(tint, vec3(mix(3.6, mix(0.38, 0.75, cool), h)));
	vec3 edge = vec3(tint.r + 0.4 * tint.b, 0.2 * tint.g, 0.75 * tint.b);
	edge /= max(max(edge.r, edge.g), max(edge.b, 1e-4));
	tone = mix(tone, edge, (1.0 - smoothstep(0.0, 0.55, h)) * cool);
	return tone * (peak * (mix(0.45, 1.0, h) + 0.5 * max(heat - 1.0, 0.0)));
}

float FireHot(float heat)
{
	return smoothstep(0.55, 1.0, heat);
}
