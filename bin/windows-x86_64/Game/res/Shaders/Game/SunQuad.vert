#version 450
#include "Sun.glsl"

layout(std430, set = 0, binding = 0) readonly buffer QuadBuffer { SunQuad Quads[]; };

layout(location = 0) out vec2 outUv;
layout(location = 1) flat out uint outIndex;
layout(location = 2) out vec3 outWorld;
layout(location = 3) flat out vec3 outRight;
layout(location = 4) flat out vec3 outUp;

const vec2 Corners[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

void main()
{
	SunQuad quad = Quads[gl_InstanceIndex];
	vec2 corner = Corners[gl_VertexIndex];
	vec3 center = quad.CenterSize.xyz;
	vec3 toward = normalize(center - pc.Eye.xyz);
	vec3 right = cross(toward, vec3(0.0, 1.0, 0.0));
	right = dot(right, right) > 1e-6 ? normalize(right) : vec3(1.0, 0.0, 0.0);
	vec3 up = cross(right, toward);
	vec3 world = center + (right * corner.x + up * corner.y) * quad.CenterSize.w;
	outUv = corner;
	outIndex = gl_InstanceIndex;
	outWorld = world;
	outRight = right;
	outUp = up;
	gl_Position = pc.ViewProjection * vec4(world, 1.0);
}
