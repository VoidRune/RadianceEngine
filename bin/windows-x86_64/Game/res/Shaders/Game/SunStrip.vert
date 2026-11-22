#version 450
#include "Sun.glsl"

layout(std430, set = 0, binding = 0) readonly buffer VertexBuffer { SunVertex Vertices[]; };

layout(location = 0) out vec2 outUv;
layout(location = 1) out vec4 outBody;
layout(location = 2) flat out vec3 outLook;
layout(location = 3) out float outEnergy;

const vec2 Corners[6] = vec2[](vec2(0.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(0.0, -1.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

void main()
{
	SunVertex first = Vertices[gl_InstanceIndex];
	if (first.Tangent.w > 0.5 || abs(first.Extra.w - pc.Screen.z) > 0.5)
	{
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
		return;
	}
	vec2 corner = Corners[gl_VertexIndex];
	SunVertex p = Vertices[gl_InstanceIndex + (corner.x > 0.5 ? 1 : 0)];
	vec3 position = p.PositionWidth.xyz;
	vec3 view = normalize(pc.Eye.xyz - position);
	vec3 side = cross(p.Tangent.xyz, view);
	float facing = length(side);
	side = facing > 1e-4 ? side / facing : normalize(cross(view, vec3(0.0, 1.0, 0.0)) + vec3(1e-4));
	float w = (pc.ViewProjection * vec4(position, 1.0)).w;
	float pixel = max(w, 1e-3) / pc.Camera.z;
	float radius = p.PositionWidth.w;
	float drawn = max(radius, 1.2 * pixel);
	outUv = vec2(p.Glow.w, corner.y);
	outBody = p.Body;
	outLook = first.Extra.xyz;
	outEnergy = radius / drawn * mix(0.3, 1.0, smoothstep(0.1, 0.4, facing));
	gl_Position = pc.ViewProjection * vec4(position + side * (corner.y * drawn), 1.0);
}
