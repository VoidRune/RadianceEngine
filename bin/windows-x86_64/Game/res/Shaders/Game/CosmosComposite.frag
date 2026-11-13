#version 450
#include "CosmosCommon.glsl"

layout(set = 0, binding = 1) uniform sampler2D Scene;
layout(set = 0, binding = 2) uniform sampler2D Bloom;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

vec3 ShadowLift(float tone)
{
	vec3 indigo = vec3(0.010, 0.008, 0.030);
	vec3 teal = vec3(0.004, 0.017, 0.024);
	vec3 plum = vec3(0.022, 0.006, 0.020);
	vec3 lift = mix(indigo, teal, smoothstep(0.4, 0.56, tone));
	return mix(lift, plum, smoothstep(0.58, 0.7, tone) * 0.8);
}

void main()
{
	vec2 centered = inUv - 0.5;
	float aspect = frame.Screen.x * frame.Screen.w;
	vec2 fringe = centered * (0.0012 + 0.012 * frame.Tint.w);
	vec3 color;
	color.r = textureLod(Scene, inUv - fringe, 0.0).r;
	vec4 middle = textureLod(Scene, inUv, 0.0);
	color.g = middle.g;
	color.b = textureLod(Scene, inUv + fringe, 0.0).b;
	vec3 bloom = textureLod(Bloom, inUv, 0.0).rgb;
	float inside = 0.0;
	float page = frame.CloudState.z;
	if (page > 0.001)
	{
		vec2 p = (inUv - frame.Cloud.xy) * vec2(aspect, 1.0);
		vec2 q = abs(p) - frame.Cloud.zw + 0.12;
		float box = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - 0.12;
		color += bloom * exp(-max(box, 0.0) / 0.09) * (1.0 - smoothstep(-0.02, -0.15, box)) * page * 0.6;
		inside = smoothstep(0.16, -0.14, box) * page;
	}
	color += bloom * 0.45;
	color *= 1.0 - 0.55 * inside;
	color /= 1.0 + 5.0 * inside * dot(color, vec3(0.3, 0.55, 0.15));
	color *= frame.Lens.z * 1.15;
	color = Tonemap(color);
	color += ShadowLift(middle.a) * (1.0 - color) * frame.Lens.w;
	float vignette = smoothstep(0.35, 1.25, length(centered * vec2(aspect, 1.0) * 1.15));
	color *= 1.0 - 0.4 * vignette;
	color *= frame.Lens.w;
	color = mix(color, vec3(0.94, 0.95, 1.0), frame.Galaxy.z);
	color = pow(color, vec3(1.0 / 2.2));
	float grain = Hash13(vec3(gl_FragCoord.xy, fract(frame.Camera.w * 7.3) * 113.0));
	float dither = Hash13(vec3(gl_FragCoord.xy * 1.37, 9.1)) + grain - 1.0;
	color += dither * (1.6 / 255.0) + (grain - 0.5) * 0.018 * (1.0 - color);
	outColor = vec4(color, 1.0);
}
