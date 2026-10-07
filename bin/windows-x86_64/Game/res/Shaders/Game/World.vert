#version 450

struct BoxInstance
{
	vec4 CenterYaw;
	vec4 HalfExtentsGround;
	vec4 ColorEmission;
};

struct MeshVertex
{
	vec4 Position;
	vec4 Normal;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances { BoxInstance instances[]; };
layout(std430, set = 0, binding = 1) readonly buffer Vertices { MeshVertex vertices[]; };

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 LightDirection;
} pc;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out float outHeight;
layout(location = 3) out float outWorldHeight;

void main()
{
	BoxInstance box = instances[gl_InstanceIndex];
	MeshVertex vertex = vertices[gl_VertexIndex];
	vec3 extent = box.HalfExtentsGround.xyz;
	vec3 local = vertex.Position.xyz * extent;
	vec3 normal = normalize(vertex.Normal.xyz / max(extent, vec3(1e-4)));

	float c = cos(box.CenterYaw.w);
	float s = sin(box.CenterYaw.w);
	vec3 world = box.CenterYaw.xyz + vec3(c * local.x - s * local.z, local.y, s * local.x + c * local.z);

	outNormal = vec3(c * normal.x - s * normal.z, normal.y, s * normal.x + c * normal.z);
	outColor = box.ColorEmission;
	outHeight = world.y - box.HalfExtentsGround.w;
	outWorldHeight = world.y;
	gl_Position = pc.ViewProjection * vec4(world, 1.0);
}
