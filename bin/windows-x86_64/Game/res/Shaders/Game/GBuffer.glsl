const vec3 DepthColor = vec3(0.0014, 0.0017, 0.0029);

vec2 OctWrap(vec2 v)
{
	return (1.0 - abs(v.yx)) * vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

vec2 EncodeNormal(vec3 normal)
{
	vec3 n = normal.xzy / (abs(normal.x) + abs(normal.y) + abs(normal.z));
	vec2 encoded = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
	return encoded * 0.5 + 0.5;
}

vec3 DecodeNormal(vec2 encoded)
{
	encoded = encoded * 2.0 - 1.0;
	vec3 n = vec3(encoded, 1.0 - abs(encoded.x) - abs(encoded.y));
	float t = max(-n.z, 0.0);
	n.xy += vec2(n.x >= 0.0 ? -t : t, n.y >= 0.0 ? -t : t);
	return normalize(n).xzy;
}

vec4 PackSurface(vec3 normal, float roughness, float metallic)
{
	return vec4(EncodeNormal(normal), roughness, 1.0 - metallic);
}

float GroundOcclusion(float heightAboveGround)
{
	return mix(0.75, 1.0, clamp(heightAboveGround + 0.2, 0.0, 1.0));
}

float DepthFade(float focusHeight, float worldHeight)
{
	return clamp((focusHeight - worldHeight - 1.6) / 2.5, 0.0, 0.88);
}

float Haze(float focusHeight, float worldHeight)
{
	return 0.92 * (1.0 - exp(-max(focusHeight - worldHeight - 2.5, 0.0) / 15.0));
}
