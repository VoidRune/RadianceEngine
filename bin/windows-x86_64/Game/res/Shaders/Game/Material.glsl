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
const uint MeshLavaGlow = 32u;
const uint MaterialSingleSided = 64u;
const uint MeshWater = 128u;
const uint MeshGround = 256u;
const uint MeshSplat = 512u;
const uint MeshVeins = 1024u;
const uint MeshAccentTrim = 2048u;
const uint MeshAccentDye = 4096u;
const uint MeshCrystal = 8192u;
const uint MeshIce = 16384u;
const uint MeshCliff = 32768u;

vec4 SampleAlbedo(Material m, vec2 uv)
{
	return texture(Textures[nonuniformEXT(m.Albedo)], uv * m.Params.z);
}
