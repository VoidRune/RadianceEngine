#version 450
#include "Glass.glsl"

struct ModelInstance
{
	vec4 Row0;
	vec4 Row1;
	vec4 Row2;
	vec4 Tint;
	vec4 Shading;
	uvec4 Info;
	vec4 Accent;
	vec4 Flash;
	vec4 Dissolve;
	vec4 Burn;
};

struct ModelVertex
{
	vec4 PositionU;
	vec4 NormalV;
	uvec4 Skin;
};

layout(std430, set = 0, binding = 0) readonly buffer FrameBuffer { GlassFrame Frame; };
layout(std430, set = 0, binding = 1) readonly buffer Instances { ModelInstance instances[]; };
layout(std430, set = 0, binding = 6) readonly buffer Vertices { ModelVertex vertices[]; };

layout(location = 0) out vec3 outWorld;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUv;
layout(location = 3) out vec4 outTangent;
layout(location = 4) flat out uint outMaterial;
layout(location = 5) out vec3 outLocal;
layout(location = 6) flat out vec4 outDissolve;
layout(location = 7) flat out vec4 outBurn;
layout(location = 8) out vec4 outColor;
layout(location = 9) flat out vec4 outOrigin;
layout(location = 10) flat out uint outFlags;
layout(location = 11) flat out vec4 outTint;
layout(location = 12) flat out vec4 outAccent;
layout(location = 13) flat out float outGlow;

vec4 UnpackTangent(uint packed)
{
	int bits = int(packed);
	vec3 t = vec3(bitfieldExtract(bits, 0, 10), bitfieldExtract(bits, 10, 10), bitfieldExtract(bits, 20, 10)) / 511.0;
	return vec4(t, (packed & 0x40000000u) != 0u ? -1.0 : 1.0);
}

void main()
{
	ModelInstance instance = instances[gl_InstanceIndex];
	ModelVertex vertex = vertices[gl_VertexIndex];
	vec4 local = vec4(vertex.PositionU.xyz, 1.0);
	vec3 world = vec3(dot(instance.Row0, local), dot(instance.Row1, local), dot(instance.Row2, local));
	vec3 normal = vertex.NormalV.xyz;
	vec4 tangent = UnpackTangent(vertex.Skin.z);
	outWorld = world;
	outNormal = vec3(dot(instance.Row0.xyz, normal), dot(instance.Row1.xyz, normal), dot(instance.Row2.xyz, normal));
	outTangent = vec4(dot(instance.Row0.xyz, tangent.xyz), dot(instance.Row1.xyz, tangent.xyz), dot(instance.Row2.xyz, tangent.xyz), tangent.w);
	outUv = vec2(vertex.PositionU.w, vertex.NormalV.w);
	outMaterial = instance.Info.y;
	outLocal = vertex.PositionU.xyz * length(vec3(instance.Row0.y, instance.Row1.y, instance.Row2.y));
	outDissolve = instance.Dissolve;
	outBurn = instance.Burn;
	outColor = unpackUnorm4x8(vertex.Skin.w);
	outOrigin = vec4(instance.Row0.w, instance.Row1.w, instance.Row2.w, instance.Shading.y);
	outFlags = instance.Info.w;
	outTint = instance.Tint;
	outAccent = instance.Accent;
	outGlow = instance.Shading.x;
	gl_Position = Frame.ViewProjection * vec4(world, 1.0);
}
