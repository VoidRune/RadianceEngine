#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_buffer_reference2 : require
#include "common.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

struct Vertex {
    vec3 pos;
    vec3 normal;
    vec2 uv;
};

struct MeshPrimitive
{
    uvec2 vertexAddress;
    uvec2 indexAddress;
    uint materialIndex;
};

struct Material
{
    vec4 color;
    vec4 emission;
    float metallic;
    float roughness;
    float transmission;
    uint textureIndex;
};

layout(set = 1, binding = 0) buffer MeshPrimitiveBuffer { MeshPrimitive meshPrimitives[]; };
layout(set = 1, binding = 1) buffer MaterialBuffer { Material materials[]; };
layout(set = 1, binding = 2) uniform sampler2D textures[];

layout(buffer_reference, scalar) readonly buffer VertexBuffer { Vertex vertices[]; };
layout(buffer_reference, scalar) readonly buffer IndexBuffer { uint indices[]; };

hitAttributeEXT vec2 attribs;

void main()
{
    uint primID = gl_PrimitiveID;

    uint instanceIndex = gl_InstanceCustomIndexEXT;
    MeshPrimitive mesh = meshPrimitives[instanceIndex];
    VertexBuffer vb = VertexBuffer(mesh.vertexAddress);
    IndexBuffer  ib = IndexBuffer(mesh.indexAddress);
    Material mat = materials[mesh.materialIndex];

    uint i0 = ib.indices[primID * 3 + 0];
    uint i1 = ib.indices[primID * 3 + 1];
    uint i2 = ib.indices[primID * 3 + 2];
    vec3 p0 = vb.vertices[i0].pos;
    vec3 p1 = vb.vertices[i1].pos;
    vec3 p2 = vb.vertices[i2].pos;
    vec3 n0 = vb.vertices[i0].normal;
    vec3 n1 = vb.vertices[i1].normal;
    vec3 n2 = vb.vertices[i2].normal;
    vec2 uv0 = vb.vertices[i0].uv;
    vec2 uv1 = vb.vertices[i1].uv;
    vec2 uv2 = vb.vertices[i2].uv;

    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 localPosition = p0 * bary.x + p1 * bary.y + p2 * bary.z;
    vec3 localNormal = n0 * bary.x + n1 * bary.y + n2 * bary.z;
    vec2 uv = uv0 * bary.x + uv1 * bary.y + uv2 * bary.z;
    vec3 position = gl_ObjectToWorldEXT * vec4(localPosition, 1.0);
    vec3 normal = normalize(gl_ObjectToWorldEXT * vec4(localNormal, 0));

    payload.color = mat.color.rgb * texture(textures[mat.textureIndex], uv).rgb;
    payload.origin = position;
    payload.normal = normal;
    payload.hitDistance = gl_HitTEXT;
    payload.didHit = 1;
    payload.emission = mat.emission.rgb;
    payload.metallic = mat.metallic;
    payload.roughness = mat.roughness;
    payload.transmission = mat.transmission;
}