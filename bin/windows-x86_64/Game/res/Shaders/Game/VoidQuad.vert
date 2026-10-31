#version 450
#include "Void.glsl"

layout(std430, set = 0, binding = 0) readonly buffer QuadBuffer { VoidQuad Quads[]; };

layout(location = 0) out vec2 outUv;
layout(location = 1) flat out uint outIndex;
layout(location = 2) out vec3 outWorld;
layout(location = 3) flat out float outCenterDepth;
layout(location = 4) flat out vec3 outRight;
layout(location = 5) flat out vec3 outUp;

const vec2 Corners[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

void main()
{
	VoidQuad quad = Quads[gl_InstanceIndex];
	vec2 corner = Corners[gl_VertexIndex];
	vec3 center = quad.CenterSize.xyz;
	float size = quad.CenterSize.w;
	vec3 right;
	vec3 up;
	if (quad.Info.x == VoidSingularity)
	{
		vec3 toward = normalize(center - pc.Eye.xyz);
		right = cross(toward, vec3(0.0, 1.0, 0.0));
		right = dot(right, right) > 1e-6 ? normalize(right) : vec3(1.0, 0.0, 0.0);
		up = cross(right, toward);
	}
	else
	{
		vec3 normal = normalize(quad.Axis.xyz);
		right = cross(vec3(0.0, 0.0, 1.0), normal);
		right = dot(right, right) > 1e-6 ? normalize(right) : vec3(1.0, 0.0, 0.0);
		up = cross(normal, right);
	}
	vec3 world = center + (right * corner.x + up * corner.y) * size;
	outUv = corner;
	outIndex = gl_InstanceIndex;
	outWorld = world;
	outCenterDepth = (pc.ViewProjection * vec4(center, 1.0)).w;
	outRight = right;
	outUp = up;
	gl_Position = pc.ViewProjection * vec4(world, 1.0);
}
