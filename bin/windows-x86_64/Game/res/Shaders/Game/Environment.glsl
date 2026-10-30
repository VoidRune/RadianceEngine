vec3 EnvironmentToLocal(vec3 d, vec4 params)
{
	return vec3(params.y * d.x - params.z * d.z, d.y, params.z * d.x + params.y * d.z);
}

vec2 EquirectUv(vec3 d)
{
	return vec2(0.5 + atan(-d.x, -d.z) * 0.15915494, acos(clamp(d.y, -1.0, 1.0)) * 0.31830989);
}

vec3 SampleEnvironment(sampler2D map, vec4 params, vec3 direction, float roughness)
{
	return textureLod(map, EquirectUv(EnvironmentToLocal(direction, params)), roughness * params.w).rgb * params.x;
}

vec3 EnvironmentIrradiance(vec4 sh[9], vec4 params, vec3 n)
{
	vec3 d = EnvironmentToLocal(n, params);
	vec3 result = sh[0].rgb * 0.282095
		+ (sh[1].rgb * d.y + sh[2].rgb * d.z + sh[3].rgb * d.x) * 0.488603
		+ (sh[4].rgb * (d.x * d.y) + sh[5].rgb * (d.y * d.z) + sh[7].rgb * (d.x * d.z)) * 1.092548
		+ sh[6].rgb * (0.315392 * (3.0 * d.z * d.z - 1.0))
		+ sh[8].rgb * (0.546274 * (d.x * d.x - d.y * d.y));
	return max(result, vec3(0.0)) * params.x;
}

float SkyOpenness(sampler2D mask, vec4 transform, vec3 world)
{
	return texture(mask, world.xz * transform.xy + transform.zw).r;
}
