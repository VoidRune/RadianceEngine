#version 450
#include "Glass.glsl"
#include "HexShield.glsl"

layout(location = 0) out vec3 outWorld;
layout(location = 1) out vec2 outLocal;
layout(location = 2) out vec2 outPlane;
layout(location = 3) out vec3 outNormal;
layout(location = 4) flat out uint outIndex;

void main()
{
	ShieldCell cell = Cells[gl_InstanceIndex];
	uint corner = uint(gl_VertexIndex) % 3u;
	uint wedge = uint(gl_VertexIndex) / 3u;
	vec2 local = vec2(0.0);
	if (corner != 0u)
	{
		float angle = float(wedge + corner - 1u) * 1.0471976;
		local = vec2(cos(angle), sin(angle));
	}
	float spin = cell.State.z;
	vec2 turned = vec2(local.x * cos(spin) - local.y * sin(spin), local.x * sin(spin) + local.y * cos(spin)) * cell.Center.w;
	vec3 right = cell.Right.xyz;
	vec3 up = cell.Up.xyz;
	vec3 normal = normalize(cross(right, up));
	vec3 world = cell.Center.xyz + right * turned.x + up * turned.y;
	if (cell.Info.z != 0u)
	{
		vec4 sphere = Shields[cell.Info.x].Sphere;
		normal = normalize(world - sphere.xyz);
		world = sphere.xyz + normal * (sphere.w + cell.Right.w);
	}
	else
		world += normal * cell.Right.w;
	outWorld = world;
	outLocal = local;
	outPlane = cell.Plane.xy + turned;
	outNormal = normal;
	outIndex = uint(gl_InstanceIndex);
	gl_Position = Frame.ViewProjection * vec4(world, 1.0);
}
