#version 450

struct ModelInstance
{
	vec4 Row0;
	vec4 Row1;
	vec4 Row2;
	vec4 Tint;
	vec4 Shading;
	uvec4 Info;
	vec4 Accent;
};

struct ModelVertex
{
	vec4 PositionU;
	vec4 NormalV;
	uvec4 Skin;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances { ModelInstance instances[]; };
layout(std430, set = 0, binding = 1) readonly buffer Vertices { ModelVertex vertices[]; };
layout(std430, set = 0, binding = 2) readonly buffer Palette { mat4 joints[]; };

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 Focus;
} pc;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUv;
layout(location = 2) flat out uvec3 outMaterial;
layout(location = 3) out vec4 outTint;
layout(location = 4) out vec3 outShading;
layout(location = 5) flat out vec2 outCut;
layout(location = 6) flat out uint outMirrored;
layout(location = 7) out vec4 outTangent;
layout(location = 8) out vec4 outColor;
layout(location = 9) out vec3 outWorld;
layout(location = 10) flat out vec4 outAccent;

const uint NoPalette = 0xFFFFFFFFu;

vec4 UnpackTangent(uint packed)
{
	int bits = int(packed);
	vec3 t = vec3(bitfieldExtract(bits, 0, 10), bitfieldExtract(bits, 10, 10), bitfieldExtract(bits, 20, 10)) / 511.0;
	return vec4(t, (packed & 0x40000000u) != 0u ? -1.0 : 1.0);
}

void main()
{
	ModelInstance instance = instances[gl_InstanceIndex];
	if (pc.Focus.y > 0.5 && instance.Shading.x >= 0.9)
	{
		gl_Position = vec4(0.0, 0.0, -1.0, 1.0);
		return;
	}
	ModelVertex vertex = vertices[gl_VertexIndex];
	vec3 position = vertex.PositionU.xyz;
	vec3 normal = vertex.NormalV.xyz;
	vec4 tangent = UnpackTangent(vertex.Skin.z);
	if (instance.Info.x != NoPalette)
	{
		uint indices = vertex.Skin.x;
		vec4 weights = unpackUnorm4x8(vertex.Skin.y);
		uint base = instance.Info.x;
		mat4 skin = joints[base + (indices & 0xFFu)] * weights.x
			+ joints[base + ((indices >> 8) & 0xFFu)] * weights.y
			+ joints[base + ((indices >> 16) & 0xFFu)] * weights.z
			+ joints[base + (indices >> 24)] * weights.w;
		position = (skin * vec4(position, 1.0)).xyz;
		normal = mat3(skin) * normal;
		tangent.xyz = mat3(skin) * tangent.xyz;
	}

	vec4 local = vec4(position, 1.0);
	vec3 world = vec3(dot(instance.Row0, local), dot(instance.Row1, local), dot(instance.Row2, local));
	bool mirrored = determinant(mat3(instance.Row0.xyz, instance.Row1.xyz, instance.Row2.xyz)) < 0.0;
	outNormal = vec3(dot(instance.Row0.xyz, normal), dot(instance.Row1.xyz, normal), dot(instance.Row2.xyz, normal));
	outTangent = vec4(dot(instance.Row0.xyz, tangent.xyz), dot(instance.Row1.xyz, tangent.xyz), dot(instance.Row2.xyz, tangent.xyz), mirrored ? -tangent.w : tangent.w);
	outUv = vec2(vertex.PositionU.w, vertex.NormalV.w);
	outMaterial = instance.Info.yzw;
	outTint = instance.Tint;
	outShading = vec3(instance.Shading.x, world.y - instance.Shading.y, world.y);
	outCut = instance.Shading.zw;
	outMirrored = mirrored ? 1u : 0u;
	outColor = unpackUnorm4x8(vertex.Skin.w);
	outWorld = world;
	outAccent = instance.Accent;
	gl_Position = pc.ViewProjection * vec4(world, 1.0);
}
