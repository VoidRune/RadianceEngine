const float PI = 3.14159265358979323846;
const float TWO_PI = 6.28318530717958647692;
const float INV_PI = 0.31830988618379067154;
const float INV_4PI = 0.07957747154594766788;
const float INF = 1e30;

struct HitPayload
{
    vec3 position;
    float t;
    vec3 geometricNormal;
    uint materialIndex;
    vec3 shadingNormal;
    uint frontFace;
    vec2 uv;
};

uint PcgHash(uint value)
{
    uint state = value * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float Random(inout uint state)
{
    state = PcgHash(state);
    return float(state >> 8) * (1.0 / 16777216.0);
}

vec2 Random2(inout uint state)
{
    return vec2(Random(state), Random(state));
}

float Luminance(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

float MaxComponent(vec3 v)
{
    return max(v.x, max(v.y, v.z));
}

float Average(vec3 v)
{
    return (v.x + v.y + v.z) * (1.0 / 3.0);
}

mat3 BuildFrame(vec3 n)
{
    float s = n.z >= 0.0 ? 1.0 : -1.0;
    float a = -1.0 / (s + n.z);
    float b = n.x * n.y * a;
    return mat3(vec3(1.0 + s * n.x * n.x * a, s * b, -s * n.x), vec3(b, s + n.y * n.y * a, -n.y), n);
}

vec3 ToLocal(mat3 frame, vec3 v)
{
    return vec3(dot(v, frame[0]), dot(v, frame[1]), dot(v, frame[2]));
}

vec3 ToWorld(mat3 frame, vec3 v)
{
    return frame * v;
}

vec2 SampleUniformDisk(vec2 u)
{
    float r = sqrt(u.x);
    float phi = TWO_PI * u.y;
    return r * vec2(cos(phi), sin(phi));
}

vec3 SampleCosineHemisphere(vec2 u)
{
    vec2 d = SampleUniformDisk(u);
    return vec3(d, sqrt(max(0.0, 1.0 - dot(d, d))));
}

vec3 SampleUniformTriangle(vec2 u)
{
    if (u.x + u.y > 1.0)
        u = 1.0 - u;
    return vec3(1.0 - u.x - u.y, u.x, u.y);
}

float PowerHeuristic(float pdfA, float pdfB)
{
    float a = pdfA * pdfA;
    float b = pdfB * pdfB;
    return a / max(a + b, 1e-30);
}

vec3 OffsetRay(vec3 p, vec3 n)
{
    const float origin = 1.0 / 32.0;
    const float floatScale = 1.0 / 65536.0;
    const float intScale = 256.0;
    ivec3 offset = ivec3(intScale * n);
    vec3 moved = vec3(
        intBitsToFloat(floatBitsToInt(p.x) + (p.x < 0.0 ? -offset.x : offset.x)),
        intBitsToFloat(floatBitsToInt(p.y) + (p.y < 0.0 ? -offset.y : offset.y)),
        intBitsToFloat(floatBitsToInt(p.z) + (p.z < 0.0 ? -offset.z : offset.z)));
    return vec3(
        abs(p.x) < origin ? p.x + floatScale * n.x : moved.x,
        abs(p.y) < origin ? p.y + floatScale * n.y : moved.y,
        abs(p.z) < origin ? p.z + floatScale * n.z : moved.z);
}
