
struct RayPayload {
  vec3 color;
  vec3 origin;
  vec3 normal;
  vec3 emission;
  float hitDistance;
  uint didHit;
  float metallic;
  float roughness;
  float transmission;
};

float RandomValue(inout uint state)
{
    //pcg hash
    state = state * 747796405u + 2891336453u;
    uint result = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    result = (result >> 22u) ^ result;
    return result / 4294967295.0;
}

vec2 RandomCircle(inout uint state) {
    float radius = sqrt(RandomValue(state));
    float angle = 6.2831853 * RandomValue(state);
    return radius * vec2(cos(angle), sin(angle));
}

vec3 RandomSphere(inout uint state) {
    vec2 disk = RandomCircle(state);
    float norm = dot(disk, disk);
    float radius = 2.0 * sqrt(1.0 - norm);
    float z = 1.0 - 2.0 * norm;
    return vec3(radius * disk, z);
}

float sampleHenyeyGreensteinAngleCosine(inout uint state, float g) {
    float g2 = g * g;
    float c = (1.0 - g2) / (1.0 - g + 2.0 * g * RandomValue(state));
    return (1.0 + g2 - c * c) / (2.0 * g);
}

vec3 sampleHenyeyGreenstein(inout uint state, float g, vec3 direction) {
    // generate random direction and adjust it so that the angle is HG-sampled
    vec3 u = RandomSphere(state);
    if (abs(g) < 1e-5) {
        return u;
    }
    float hgcos = sampleHenyeyGreensteinAngleCosine(state, g);
    vec3 circle = normalize(u - dot(u, direction) * direction);
    return sqrt(1.0 - hgcos * hgcos) * circle + hgcos * direction;
}