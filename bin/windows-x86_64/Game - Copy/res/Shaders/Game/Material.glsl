#extension GL_EXT_nonuniform_qualifier : require

struct Material
{
	uint Albedo;
	uint Surface;
	uint Normal;
	uint Flags;
	vec4 Color;
	vec4 Params;
	vec4 Extra;
};

layout(std430, set = 0, binding = 3) readonly buffer Materials { Material materials[]; };
layout(set = 1, binding = 0) uniform sampler2D Textures[];

const uint MaterialAlphaTest = 1u;
const uint MaterialPbr = 2u;
const uint MeshLayered = 4u;
const uint MeshWet = 8u;
const uint MeshVertexTint = 16u;

vec4 SampleAlbedo(Material m, vec2 uv)
{
	return texture(Textures[nonuniformEXT(m.Albedo)], uv * m.Params.z);
}
