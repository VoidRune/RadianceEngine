#version 450
#include "Lake.glsl"

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

layout(std430, set = 0, binding = 0) readonly buffer FrameBuffer { LakeFrame Frame; };
layout(std430, set = 0, binding = 1) readonly buffer Instances { ModelInstance instances[]; };
layout(std430, set = 0, binding = 2) readonly buffer Vertices { ModelVertex vertices[]; };
layout(std430, set = 0, binding = 6) readonly buffer Palette { mat4 joints[]; };

layout(location = 0) out vec3 outWorld;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUv;
layout(location = 3) out vec4 outColor;
layout(location = 4) flat out uvec2 outMaterial;
layout(location = 5) flat out uint outFlags;
layout(location = 6) out vec4 outTint;
layout(location = 7) flat out vec4 outGlow;
layout(location = 8) out vec3 outLocal;
layout(location = 9) out float outPulse;
layout(location = 10) out vec4 outTangent;

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
	vec3 position = vertex.PositionU.xyz;
	vec3 normal = vertex.NormalV.xyz;
	vec4 tangent = UnpackTangent(vertex.Skin.z);
	vec4 swim = instance.Dissolve;
	if (swim.x > 0.0)
	{
		float along = position.z * swim.w;
		float rear = 1.0 - smoothstep(-1.0, swim.z, along);
		float phase = position.z * swim.y + instance.Burn.x;
		position.x += sin(phase) * swim.x * rear;
		float slope = cos(phase) * swim.x * swim.y * rear;
		normal = normalize(normal + vec3(0.0, 0.0, -slope * normal.x));
	}
	if (instance.Burn.y > 0.0)
	{
		float beat = sin(instance.Burn.z);
		float bell = clamp(position.y / instance.Burn.y, 0.0, 1.0);
		position.xz *= 1.0 + beat * (0.28 * (1.0 - bell));
		position.y *= 1.0 - beat * 0.14;
	}
	if (instance.Info.x != 0xFFFFFFFFu)
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
	if (instance.Flash.w > 1.5)
	{
		uint sway = vertex.Skin.x;
		float weight = float(sway & 0xFFu) / 255.0;
		float phase = float((sway >> 8) & 0xFFu) / 255.0 * 6.2832;
		float reach = float((sway >> 16) & 0xFFu) * 0.01 * weight * weight;
		float t = instance.Flash.z;
		vec2 drift = vec2(sin(t * 0.53 + phase + position.x * 0.13), cos(t * 0.41 + phase * 1.3 + position.z * 0.11));
		vec2 flutter = vec2(sin(t * 1.9 + phase * 3.0 + weight * 4.0), cos(t * 1.7 + phase * 2.0 + weight * 3.0)) * 0.22;
		vec2 offset = (drift + flutter) * reach;
		position.xz += offset;
		position.y -= dot(offset, offset) * 0.35 / max(reach + 0.2, 0.2);
	}
	else if (instance.Flash.w > 0.5)
	{
		float rate = 0.12 + 0.16 * fract(position.x * 7.31 + position.z * 3.17);
		float y = instance.Flash.x - mod(instance.Flash.x - position.y + instance.Flash.z * rate, instance.Flash.y);
		position = vec3(position.x + 0.25 * sin(instance.Flash.z * 0.21 + position.z * 1.7), y, position.z + 0.25 * cos(instance.Flash.z * 0.17 + position.x * 1.3)) + normal;
		normal = vec3(0.0, 1.0, 0.0);
	}
	vec4 local = vec4(position, 1.0);
	vec3 world = vec3(dot(instance.Row0, local), dot(instance.Row1, local), dot(instance.Row2, local));
	float kind = vertex.NormalV.w;
	if ((instance.Info.w & MeshUnderGlow) != 0u && kind > 13.5 && kind < 16.5)
	{
		vec3 view = normalize(world - Frame.Eye.xyz);
		vec3 right = normalize(cross(view, vec3(0.0, 1.0, 0.0)));
		vec3 upward = cross(right, view);
		vec2 corner = vec2(vertex.PositionU.w, kind - 15.0) * vertex.NormalV.x;
		world += right * corner.x + upward * corner.y;
		normal = -view;
	}
	outWorld = world;
	outLocal = vertex.PositionU.xyz;
	float wave = 0.5 + 0.5 * sin(vertex.PositionU.z * 0.95 + instance.Burn.w * 1.9);
	outPulse = instance.Burn.w > 0.0 ? 1.0 + 1.1 * wave * wave * wave : 1.0;
	outNormal = vec3(dot(instance.Row0.xyz, normal), dot(instance.Row1.xyz, normal), dot(instance.Row2.xyz, normal));
	outUv = vec2(vertex.PositionU.w, vertex.NormalV.w);
	bool mirrored = determinant(mat3(instance.Row0.xyz, instance.Row1.xyz, instance.Row2.xyz)) < 0.0;
	outTangent = vec4(dot(instance.Row0.xyz, tangent.xyz), dot(instance.Row1.xyz, tangent.xyz), dot(instance.Row2.xyz, tangent.xyz), mirrored ? -tangent.w : tangent.w);
	outColor = unpackUnorm4x8(vertex.Skin.w);
	outMaterial = instance.Info.yz;
	outFlags = instance.Info.w;
	outTint = instance.Tint;
	outGlow = vec4(instance.Accent.rgb, instance.Shading.x);
	vec3 seen = world;
	seen.y = Frame.Plane.x + min(world.y - Frame.Plane.x, 0.0) * SquashDepth + max(world.y - Frame.Plane.x, 0.0);
	gl_Position = Frame.ViewProjection * vec4(seen, 1.0);
}
