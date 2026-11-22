#version 450
#include "Portal.glsl"

layout(location = 0) out vec2 outLocal;
layout(location = 1) flat out uint outIndex;
layout(location = 2) out vec3 outWorld;

const vec2 Corners[6] = vec2[](vec2(-1.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(-1.0, 0.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

void main()
{
	PortalSheet sheet = Sheets[gl_InstanceIndex];
	vec2 corner = Corners[gl_VertexIndex];
	float x = corner.x * (sheet.BaseHalf.w + PortalMargin);
	float y = mix(-PortalMargin, sheet.NormalApex.w + PortalMargin, corner.y);
	vec3 world = sheet.BaseHalf.xyz + sheet.RightSpring.xyz * x + vec3(0.0, y, 0.0);
	outLocal = vec2(x, y);
	outIndex = gl_InstanceIndex;
	outWorld = world;
	gl_Position = pc.ViewProjection * vec4(world, 1.0);
}
