struct Vertex
{
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec4 tangent;
};

struct MeshPrimitive
{
    uvec2 vertexAddress;
    uvec2 indexAddress;
    uint materialIndex;
};

struct Material
{
    vec3 baseColor;
    float metallic;
    vec3 emission;
    float roughness;
    vec3 sheen;
    float transmission;
    vec3 specularColor;
    float alpha;
    float ior;
    float clearcoat;
    float clearcoatRoughness;
    float normalScale;
    uint baseColorTexture;
    uint metallicRoughnessTexture;
    uint normalTexture;
    uint emissiveTexture;
    vec4 uvTransform;
    vec2 uvOffset;
    float alphaCutoff;
    int mediumIndex;
    uint flags;
    float emissionLuminance;
    uint pad0;
    uint pad1;
};

struct Medium
{
    vec3 sigmaA;
    float g;
    vec3 sigmaS;
    float pad;
};

struct EnvironmentEntry
{
    float probability;
    uint alias;
    float density;
};

struct LightTriangle
{
    vec3 p0;
    float probability;
    vec3 p1;
    uint materialIndex;
    vec3 p2;
    uint alias;
    vec2 uv0;
    vec2 uv1;
    vec2 uv2;
    vec2 pad;
};

const uint MATERIAL_NULL_SURFACE = 1u;
const uint MATERIAL_ALPHA_MASK = 2u;
const uint MATERIAL_ALPHA_BLEND = 4u;
const uint MATERIAL_SPECULAR_GLOSSINESS = 8u;
const uint MATERIAL_DOUBLE_SIDED = 16u;
const uint MATERIAL_THIN_WALLED = 32u;

layout(set = 1, binding = 0) readonly buffer MeshPrimitiveBuffer { MeshPrimitive meshPrimitives[]; };
layout(set = 1, binding = 1) readonly buffer MaterialBuffer { Material materials[]; };
layout(set = 1, binding = 2) readonly buffer LightBuffer
{
    uint lightCount;
    float totalLightPower;
    uint lightPad0;
    uint lightPad1;
    LightTriangle lights[];
};
layout(set = 1, binding = 3) readonly buffer MediumBuffer
{
    int globalMedium;
    uint mediumCount;
    uint mediumPad0;
    uint mediumPad1;
    Medium media[];
};
layout(set = 1, binding = 4) readonly buffer EnvironmentBuffer
{
    uint environmentWidth;
    uint environmentHeight;
    float environmentTotal;
    float environmentIntensity;
    float environmentRotationCos;
    float environmentRotationSin;
    uint environmentPad0;
    uint environmentPad1;
    EnvironmentEntry environmentEntries[];
};
layout(set = 1, binding = 5) uniform sampler2D environmentMap;
layout(set = 1, binding = 6) uniform sampler2D textures[];

vec2 MaterialUv(Material material, vec2 uv)
{
    return mat2(material.uvTransform.xy, material.uvTransform.zw) * uv + material.uvOffset;
}

vec4 SampleTexture(uint index, vec2 uv)
{
    return textureLod(textures[nonuniformEXT(index)], uv, 0.0);
}

float MaterialAlpha(Material material, vec2 uv)
{
    return material.alpha * SampleTexture(material.baseColorTexture, uv).a;
}

vec3 EnvironmentToLocal(vec3 d)
{
    return vec3(d.x * environmentRotationCos - d.z * environmentRotationSin, d.y, d.x * environmentRotationSin + d.z * environmentRotationCos);
}

vec3 EnvironmentToWorld(vec3 d)
{
    return vec3(d.x * environmentRotationCos + d.z * environmentRotationSin, d.y, d.z * environmentRotationCos - d.x * environmentRotationSin);
}

vec2 DirectionToEquirect(vec3 d)
{
    return vec2(0.5 + atan(d.x, d.z) * (0.5 * INV_PI), acos(clamp(d.y, -1.0, 1.0)) * INV_PI);
}

vec3 EquirectToDirection(vec2 uv)
{
    float phi = (uv.x - 0.5) * TWO_PI;
    float theta = uv.y * PI;
    float sinTheta = sin(theta);
    return vec3(sinTheta * sin(phi), cos(theta), sinTheta * cos(phi));
}

vec3 EnvironmentRadiance(vec3 direction)
{
    return textureLod(environmentMap, DirectionToEquirect(EnvironmentToLocal(direction)), 0.0).rgb * environmentIntensity;
}
