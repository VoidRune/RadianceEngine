#version 450

layout(set = 0, binding = 0) uniform sampler2D Source;

layout(push_constant) uniform PushConstants
{
	vec4 Size;
	float Sharpness;
} pc;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main()
{
	vec2 position = inUv * pc.Size.xy;
	vec2 center = floor(position - 0.5) + 0.5;
	vec2 f = position - center;
	vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
	vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
	vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
	vec2 w3 = f * f * (-0.5 + 0.5 * f);
	vec2 w12 = w1 + w2;
	vec2 t0 = (center - 1.0) * pc.Size.zw;
	vec2 t3 = (center + 2.0) * pc.Size.zw;
	vec2 t12 = (center + w2 / w12) * pc.Size.zw;
	vec2 corner = (center + 0.5) * pc.Size.zw;
	vec4 red = textureGather(Source, corner, 0);
	vec4 green = textureGather(Source, corner, 1);
	vec4 blue = textureGather(Source, corner, 2);
	vec3 a = vec3(red.w, green.w, blue.w);
	vec3 b = vec3(red.z, green.z, blue.z);
	vec3 c = vec3(red.x, green.x, blue.x);
	vec3 d = vec3(red.y, green.y, blue.y);
	vec4 sum = vec4(a * (w1.x * w1.y) + b * (w2.x * w1.y) + c * (w1.x * w2.y) + d * (w2.x * w2.y), w12.x * w12.y);
	sum += vec4(textureLod(Source, vec2(t12.x, t0.y), 0.0).rgb, 1.0) * (w12.x * w0.y);
	sum += vec4(textureLod(Source, vec2(t0.x, t12.y), 0.0).rgb, 1.0) * (w0.x * w12.y);
	sum += vec4(textureLod(Source, vec2(t3.x, t12.y), 0.0).rgb, 1.0) * (w3.x * w12.y);
	sum += vec4(textureLod(Source, vec2(t12.x, t3.y), 0.0).rgb, 1.0) * (w12.x * w3.y);
	vec3 color = sum.rgb / sum.a;
	vec3 low = min(min(a, b), min(c, d));
	vec3 high = max(max(a, b), max(c, d));
	vec3 bilinear = mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
	vec3 contrast = high - low;
	color += (color - bilinear) * (pc.Sharpness * (1.0 - max(contrast.r, max(contrast.g, contrast.b))));
	outColor = vec4(clamp(color, low, high), 1.0);
}
