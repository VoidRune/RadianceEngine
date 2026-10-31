#version 450
#include "Lightning.glsl"

layout(std430, set = 0, binding = 0) readonly buffer VertexBuffer { LightningVertex Vertices[]; };

layout(location = 0) out vec2 outUv;
layout(location = 1) out vec3 outGlow;
layout(location = 2) out vec3 outCore;
layout(location = 3) out vec2 outShape;

const vec2 Corners[6] = vec2[](vec2(0.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(0.0, -1.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

void main()
{
	if (Vertices[gl_InstanceIndex].Tangent.w > 0.5)
	{
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
		return;
	}
	vec2 corner = Corners[gl_VertexIndex];
	LightningVertex p = Vertices[gl_InstanceIndex + (corner.x > 0.5 ? 1 : 0)];
	vec3 position = p.PositionWidth.xyz;
	vec3 view = normalize(pc.Eye.xyz - position);
	vec3 side = cross(p.Tangent.xyz, view);
	float facing = length(side);
	side = facing > 1e-4 ? side / facing : pc.Right.xyz;
	float w = (pc.ViewProjection * vec4(position, 1.0)).w;
	float pixel = max(w, 1e-3) / pc.Camera.z;
	float radius = p.PositionWidth.w;
	float drawn = max(radius, 3.0 * pixel);
	float core = p.Glow.w * radius;
	float coreDrawn = max(core, 0.7 * pixel);
	outUv = vec2(p.Core.w, corner.y);
	outGlow = p.Glow.rgb * (radius / drawn);
	outCore = p.Core.rgb;
	outShape = vec2(coreDrawn / drawn, core > 0.0 ? core / coreDrawn : 0.0);
	gl_Position = pc.ViewProjection * vec4(position + side * (corner.y * drawn), 1.0);
}
