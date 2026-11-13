#version 450
#include "UiCommon.glsl"

layout(set = 0, binding = 2) uniform sampler2D BackdropLight;

vec3 Hash32(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
	p3 += dot(p3, p3.yxz + 33.33);
	return fract((p3.xxy + p3.yzz) * p3.zyx);
}

vec3 Stars(vec2 p, float scale, float seed, float time)
{
	vec2 g = p * scale;
	vec2 cell = floor(g);
	vec3 h = Hash32(cell + seed);
	if (h.z > 0.16)
		return vec3(0.0);
	vec2 center = 0.2 + 0.6 * h.xy;
	float distance = length(g - cell - center) * pc.ScreenSize.y / scale;
	float twinkle = 0.55 + 0.45 * sin(time * (0.8 + 2.4 * h.x) + h.y * 40.0);
	float bright = 0.25 + 1.6 * pow(h.y, 6.0);
	vec3 tone = mix(vec3(1.0, 0.86, 0.7), vec3(0.7, 0.85, 1.0), h.x);
	return tone * bright * twinkle * exp(-distance * distance * 1.1);
}

void main()
{
	Primitive primitive = primitives[inIndex];
	float appear = primitive.Params.x;
	float time = pc.Time;
	vec2 uv = inScreen / pc.ScreenSize;
	vec2 p = (inScreen - pc.ScreenSize * 0.5) / pc.ScreenSize.y;
	vec4 nebula = pc.Backdrop > 0.5 ? texture(BackdropLight, uv) : vec4(0.0, 0.0, 0.0, smoothstep(0.12, 0.7, length(p)));
	float calm = nebula.a;
	float spin = time * 0.006;
	vec2 drift = mat2(cos(spin), sin(spin), -sin(spin), cos(spin)) * p;
	vec3 color = nebula.rgb;
	color += Stars(drift, 70.0, 1.0, time);
	color += Stars(drift * 1.37 + vec2(time * 0.002, 0.0), 150.0, 17.0, time) * 0.6;
	float alpha = appear * (0.9 - 0.06 * calm);
	vec3 deep = vec3(0.006, 0.007, 0.016);
	outColor = Finish(primitive, vec4(deep * alpha + color * appear * (1.0 - 0.4 * (1.0 - calm)), alpha));
}
