struct Vertex
{
    vec3 position;
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
    vec3 baseColor;
    float metallic;
    vec3 emission;
    float roughness;
    vec3 sheen;
    float transmission;
    float ior;
    float clearcoat;
    float clearcoatRoughness;
    uint textureIndex;
    int mediumIndex;
    uint flags;
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

struct LightTriangle
{
    vec3 p0;
    float cdf;
    vec3 p1;
    float pad0;
    vec3 p2;
    float pad1;
    vec3 emission;
    float pad2;
};

const uint MATERIAL_NULL_SURFACE = 1u;

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
layout(set = 1, binding = 4) uniform sampler2D textures[];

vec3 EnvironmentRadiance(vec3 dir)
{
    const vec3 sunDir = normalize(vec3(1.0));
    const vec3 groundColor = vec3(0.9);
    const vec3 horizonColor = vec3(1.0);
    const vec3 zenithColor = vec3(0.08, 0.37, 0.73);

    float sun = pow(max(0.0, dot(dir, sunDir)), 100.0) * 100.0;
    float skyGradient = pow(smoothstep(0.0, 0.4, dir.y), 0.45);
    float groundToSky = smoothstep(-0.01, 0.0, dir.y);
    vec3 sky = mix(horizonColor, zenithColor, skyGradient);
    return mix(groundColor, sky, groundToSky) + sun * float(groundToSky >= 1.0);
}
