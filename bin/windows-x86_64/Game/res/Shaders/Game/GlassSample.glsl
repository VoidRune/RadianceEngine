float LinearDepth(float depth)
{
	float near = Frame.Forward.w;
	float far = Frame.Screen.z;
	return near * far / (far - depth * (far - near));
}

vec2 ToScreen(vec3 world)
{
	vec4 clip = Frame.ViewProjection * vec4(world, 1.0);
	return clip.xy / max(clip.w, 1e-3) * 0.5 + 0.5;
}

vec3 PyramidLevel(int level, vec2 uv)
{
	vec4 rect = Frame.Levels[level];
	vec2 inset = 0.5 * Frame.Atlas.xy;
	return textureLod(Pyramid, rect.xy + clamp(uv * rect.zw, inset, rect.zw - inset), 0.0).rgb;
}

vec3 SamplePyramid(vec2 uv, float level)
{
	uv = clamp(uv, Frame.Region.xy, Frame.Region.zw);
	level = clamp(level, 0.0, 3.0);
	float base = floor(level);
	vec3 color = PyramidLevel(int(base), uv);
	float t = level - base;
	if (t > 0.01)
		color = mix(color, PyramidLevel(int(base) + 1, uv), t);
	return color;
}

vec3 Sky(vec3 direction, float roughness)
{
	if (Frame.Environment.x > 0.0)
		return SampleEnvironment(EnvironmentMap, Frame.Environment, direction, roughness);
	float sun = max(dot(direction, Frame.SunDirection.xyz), 0.0);
	float sun5 = sun * sun;
	sun5 *= sun5 * sun;
	float sun40 = sun5 * sun5;
	sun40 *= sun40;
	sun40 *= sun40;
	return HemisphereRadiance(Frame.SkyRadiance.rgb, Frame.GroundRadiance.rgb, direction.y, 1.0 / max(roughness, 0.08))
		+ Frame.SunRadiance.rgb * (0.05 * sun5 + 0.3 * sun40);
}
