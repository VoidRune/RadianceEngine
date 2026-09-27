#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_buffer_reference2 : require
#include "common.glsl"
#include "scene.glsl"

layout(location = 0) rayPayloadInEXT HitPayload payload;

layout(buffer_reference, scalar) readonly buffer VertexBuffer { Vertex vertices[]; };
layout(buffer_reference, scalar) readonly buffer IndexBuffer { uint indices[]; };

hitAttributeEXT vec2 attribs;

void main()
{
    MeshPrimitive mesh = meshPrimitives[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    payload.t = gl_HitTEXT;
    payload.materialIndex = mesh.materialIndex;
    if (payload.shadowQuery != 0u && (materials[mesh.materialIndex].flags & (MATERIAL_NULL_SURFACE | MATERIAL_THIN_WALLED)) == 0u)
        return;

    VertexBuffer vertexBuffer = VertexBuffer(mesh.vertexAddress);
    IndexBuffer indexBuffer = IndexBuffer(mesh.indexAddress);

    uint baseIndex = gl_PrimitiveID * 3;
    Vertex v0 = vertexBuffer.vertices[indexBuffer.indices[baseIndex + 0]];
    Vertex v1 = vertexBuffer.vertices[indexBuffer.indices[baseIndex + 1]];
    Vertex v2 = vertexBuffer.vertices[indexBuffer.indices[baseIndex + 2]];

    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 localPosition = v0.position * bary.x + v1.position * bary.y + v2.position * bary.z;
    vec3 localNormal = v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z;
    vec3 localTangent = v0.tangent.xyz * bary.x + v1.tangent.xyz * bary.y + v2.tangent.xyz * bary.z;
    vec3 localGeometricNormal = cross(v1.position - v0.position, v2.position - v0.position);

    mat3 objectToWorld = mat3(gl_ObjectToWorldEXT);
    vec3 geometricNormal = normalize(vec3(localGeometricNormal * gl_WorldToObjectEXT));
    vec3 shadingNormal = dot(localNormal, localNormal) > 0.0 ? normalize(vec3(localNormal * gl_WorldToObjectEXT)) : geometricNormal;
    float handedness = (v0.tangent.w < 0.0) != (determinant(objectToWorld) < 0.0) ? -1.0 : 1.0;

    payload.position = gl_ObjectToWorldEXT * vec4(localPosition, 1.0);
    payload.geometricNormal = geometricNormal;
    payload.shadingNormal = shadingNormal;
    payload.frontFace = dot(gl_WorldRayDirectionEXT, geometricNormal) < 0.0 ? 1u : 0u;
    payload.tangent = vec4(objectToWorld * localTangent, handedness);
    payload.uv = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;
}
