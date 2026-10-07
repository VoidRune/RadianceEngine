#version 450

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 LightDirection;
} pc;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inHeight;
layout(location = 3) in float inWorldHeight;

layout(location = 0) out vec4 outColor;

const vec3 DepthColor = vec3(0.0014, 0.0017, 0.0029);

void main()
{
	vec3 normal = normalize(inNormal);
	float diffuse = max(dot(normal, -pc.LightDirection.xyz), 0.0);
	float sky = 0.5 + 0.5 * normal.y;
	float occlusion = mix(0.75, 1.0, clamp(inHeight + 0.2, 0.0, 1.0));
	vec3 lit = inColor.rgb * (0.18 + 0.22 * sky + 0.75 * diffuse) * occlusion;
	vec3 color = mix(lit, inColor.rgb, clamp(inColor.a, 0.0, 1.0));
	float depth = clamp((pc.LightDirection.w - inWorldHeight - 1.6) / 2.5, 0.0, 0.88);
	color = mix(color, DepthColor, depth);
	outColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
