#version 450

layout(set = 0, binding = 0) uniform sampler2D Source;

layout(push_constant) uniform PushConstants
{
	vec4 Size;
	vec2 Ratio;
} pc;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main()
{
	vec2 center = inUv * pc.Size.xy;
	vec2 low = center - 0.5 * pc.Ratio;
	vec2 high = center + 0.5 * pc.Ratio;
	ivec2 first = ivec2(floor(low + 1e-3));
	ivec2 last = ivec2(ceil(high - 1e-3)) - 1;
	ivec2 edge = ivec2(pc.Size.xy) - 1;
	vec3 sum = vec3(0.0);
	float total = 0.0;
	for (int y = first.y; y <= last.y; y++)
	{
		float wy = min(high.y, float(y + 1)) - max(low.y, float(y));
		for (int x = first.x; x <= last.x; x++)
		{
			float w = (min(high.x, float(x + 1)) - max(low.x, float(x))) * wy;
			sum += pow(texelFetch(Source, clamp(ivec2(x, y), ivec2(0), edge), 0).rgb, vec3(2.2)) * w;
			total += w;
		}
	}
	outColor = vec4(pow(sum / max(total, 1e-6), vec3(1.0 / 2.2)), 1.0);
}
