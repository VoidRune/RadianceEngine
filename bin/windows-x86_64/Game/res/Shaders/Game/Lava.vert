#version 450

struct BoxInstance
{
	vec4 CenterYaw;
	vec4 HalfExtentsGround;
	vec4 ColorEmission;
	vec4 Surface;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances { BoxInstance instances[]; };

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 Params;
} pc;

layout(location = 0) out vec3 outWorld;
layout(location = 1) flat out float outTime;

const vec2 Corners[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

void main()
{
	BoxInstance quad = instances[gl_InstanceIndex];
	vec2 corner = Corners[gl_VertexIndex];
	vec3 world = quad.CenterYaw.xyz + vec3(corner.x * quad.HalfExtentsGround.x, 0.0, corner.y * quad.HalfExtentsGround.z);
	outWorld = world;
	outTime = quad.ColorEmission.x;
	gl_Position = pc.ViewProjection * vec4(world, 1.0);
}
