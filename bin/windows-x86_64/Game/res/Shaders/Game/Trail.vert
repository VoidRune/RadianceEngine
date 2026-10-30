#version 450
#include "Trail.glsl"

layout(std430, set = 0, binding = 0) readonly buffer PointBuffer { TrailPoint Points[]; };

layout(location = 0) out vec2 outUv;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec2 outAge;
layout(location = 3) flat out uint outTrail;

const vec2 Corners[6] = vec2[](vec2(0.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(0.0, -1.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

void main()
{
	TrailPoint a = Points[gl_InstanceIndex];
	if (a.Extra.w > 0.5)
	{
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
		return;
	}
	vec2 corner = Corners[gl_VertexIndex];
	TrailPoint p = corner.x > 0.5 ? Points[gl_InstanceIndex + 1] : a;
	vec3 position = p.PositionWidth.xyz;
	vec3 view = normalize(pc.Eye.xyz - position);
	vec3 side = cross(p.Tangent.xyz, view);
	float facing = length(side);
	side = facing > 1e-4 ? side / facing : pc.Right.xyz;
	float w = (pc.ViewProjection * vec4(position, 1.0)).w;
	float pixel = max(w, 1e-3) / pc.Camera.z;
	float radius = p.PositionWidth.w;
	float drawn = max(radius, 1.25 * pixel);
	vec4 color = p.Color;
	color.a *= (radius / drawn) * smoothstep(0.04, 0.25, facing);
	outUv = vec2(p.Extra.x, corner.y);
	outColor = color;
	outAge = vec2(p.Extra.y, p.Tangent.w);
	outTrail = uint(p.Extra.z + 0.5);
	gl_Position = pc.ViewProjection * vec4(position + side * (corner.y * drawn), 1.0);
}
