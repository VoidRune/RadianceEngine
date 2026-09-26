const float MIN_ALPHA = 1e-3;
const float COAT_IOR = 1.5;
const float NEE_MIN_ALPHA = 0.0025;

const float GGX_ALBEDO[64] = float[](
    0.9796, 0.9987, 0.9999, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
    0.9990, 0.8918, 0.9643, 0.9924, 0.9976, 0.9989, 0.9994, 0.9996,
    0.9997, 0.9490, 0.8864, 0.9164, 0.9595, 0.9803, 0.9887, 0.9924,
    0.9997, 0.9666, 0.9028, 0.8637, 0.8756, 0.9099, 0.9387, 0.9566,
    0.9997, 0.9649, 0.9007, 0.8396, 0.8051, 0.8046, 0.8265, 0.8548,
    0.9995, 0.9545, 0.8759, 0.7966, 0.7323, 0.6911, 0.6743, 0.6779,
    0.9993, 0.9390, 0.8366, 0.7330, 0.6423, 0.5692, 0.5142, 0.4758,
    0.9991, 0.9202, 0.7892, 0.6579, 0.5425, 0.4466, 0.3690, 0.3069);

struct SurfaceMaterial
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
    int medium;
    bool nullSurface;
};

struct Bsdf
{
    vec3 baseColor;
    vec3 sheen;
    vec3 glassTint;
    vec3 metalScale;
    float diffuseScale;
    float alpha;
    float coatAlpha;
    float ior;
    float clearcoat;
    float wMetal;
    float wOpaque;
    float wGlass;
    float pCoat;
    float pSpecular;
    float pMetal;
    float pGlass;
    float pDiffuse;
};

float FresnelDielectric(float cosThetaI, float eta)
{
    cosThetaI = clamp(cosThetaI, -1.0, 1.0);
    if (cosThetaI < 0.0)
    {
        eta = 1.0 / eta;
        cosThetaI = -cosThetaI;
    }
    float sin2ThetaT = (1.0 - cosThetaI * cosThetaI) / (eta * eta);
    if (sin2ThetaT >= 1.0)
        return 1.0;
    float cosThetaT = sqrt(max(0.0, 1.0 - sin2ThetaT));
    float rParallel = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
    float rPerpendicular = (cosThetaI - eta * cosThetaT) / (cosThetaI + eta * cosThetaT);
    return 0.5 * (rParallel * rParallel + rPerpendicular * rPerpendicular);
}

float FresnelDiffuseReflectance(float eta)
{
    float i = 1.0 / eta;
    return 0.919317 + i * (-3.4793 + i * (6.75335 + i * (-7.80989 + i * (4.98554 - 1.36881 * i))));
}

float SchlickWeight(float cosTheta)
{
    float m = clamp(1.0 - cosTheta, 0.0, 1.0);
    float m2 = m * m;
    return m2 * m2 * m;
}

vec3 FresnelSchlick(vec3 f0, float cosTheta)
{
    return f0 + (1.0 - f0) * SchlickWeight(cosTheta);
}

float GgxD(vec3 m, float alpha)
{
    float a2 = alpha * alpha;
    float d = m.z * m.z * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

float GgxLambda(vec3 w, float alpha)
{
    float cos2 = max(w.z * w.z, 1e-12);
    float tan2 = max(0.0, 1.0 - cos2) / cos2;
    return 0.5 * (-1.0 + sqrt(1.0 + alpha * alpha * tan2));
}

float GgxG1(vec3 w, float alpha)
{
    return 1.0 / (1.0 + GgxLambda(w, alpha));
}

float GgxG2(vec3 wo, vec3 wi, float alpha)
{
    return 1.0 / (1.0 + GgxLambda(wo, alpha) + GgxLambda(wi, alpha));
}

float GgxAlbedo(float cosTheta, float alpha)
{
    vec2 p = vec2(sqrt(clamp(cosTheta, 0.0, 1.0)), min(sqrt(alpha), 1.0)) * 7.0;
    ivec2 i = min(ivec2(p), ivec2(6));
    vec2 t = p - vec2(i);
    int row = i.y * 8 + i.x;
    float e0 = mix(GGX_ALBEDO[row], GGX_ALBEDO[row + 1], t.x);
    float e1 = mix(GGX_ALBEDO[row + 8], GGX_ALBEDO[row + 9], t.x);
    return mix(e0, e1, t.y);
}

float GgxVisiblePdf(vec3 w, vec3 m, float alpha)
{
    return GgxG1(w, alpha) / max(abs(w.z), 1e-8) * GgxD(m, alpha) * abs(dot(w, m));
}

vec3 SampleGgxVisibleNormal(vec3 w, float alpha, vec2 u)
{
    vec3 wh = normalize(vec3(alpha * w.x, alpha * w.y, w.z));
    if (wh.z < 0.0)
        wh = -wh;
    vec3 t1 = wh.z < 0.99999 ? normalize(cross(vec3(0.0, 0.0, 1.0), wh)) : vec3(1.0, 0.0, 0.0);
    vec3 t2 = cross(wh, t1);
    vec2 p = SampleUniformDisk(u);
    float h = sqrt(max(0.0, 1.0 - p.x * p.x));
    p.y = mix(h, p.y, (1.0 + wh.z) * 0.5);
    float pz = sqrt(max(0.0, 1.0 - dot(p, p)));
    vec3 nh = p.x * t1 + p.y * t2 + pz * wh;
    return normalize(vec3(alpha * nh.x, alpha * nh.y, max(1e-6, nh.z)));
}

bool Refract(vec3 w, vec3 n, float eta, out vec3 wt)
{
    float cosThetaI = dot(n, w);
    if (cosThetaI < 0.0)
    {
        eta = 1.0 / eta;
        cosThetaI = -cosThetaI;
        n = -n;
    }
    float sin2ThetaT = max(0.0, 1.0 - cosThetaI * cosThetaI) / (eta * eta);
    if (sin2ThetaT >= 1.0)
        return false;
    float cosThetaT = sqrt(max(0.0, 1.0 - sin2ThetaT));
    wt = -w / eta + (cosThetaI / eta - cosThetaT) * n;
    return true;
}

vec3 EvalDielectric(vec3 wo, vec3 wi, float alpha, float eta, vec3 tint, out float pdf)
{
    pdf = 0.0;
    float cosO = wo.z;
    float cosI = wi.z;
    bool reflection = cosO * cosI > 0.0;
    float etap = reflection ? 1.0 : (cosO > 0.0 ? eta : 1.0 / eta);
    vec3 wm = wi * etap + wo;
    if (cosO == 0.0 || cosI == 0.0 || dot(wm, wm) == 0.0)
        return vec3(0.0);
    wm = normalize(wm);
    if (wm.z < 0.0)
        wm = -wm;
    if (dot(wm, wi) * cosI < 0.0 || dot(wm, wo) * cosO < 0.0)
        return vec3(0.0);

    float F = FresnelDielectric(dot(wo, wm), eta);
    if (reflection)
    {
        pdf = GgxVisiblePdf(wo, wm, alpha) / (4.0 * abs(dot(wo, wm))) * F;
        return vec3(GgxD(wm, alpha) * GgxG2(wo, wi, alpha) * F / abs(4.0 * cosI * cosO));
    }

    float denom = dot(wi, wm) + dot(wo, wm) / etap;
    denom *= denom;
    pdf = GgxVisiblePdf(wo, wm, alpha) * abs(dot(wi, wm)) / denom * (1.0 - F);
    float ft = GgxD(wm, alpha) * (1.0 - F) * GgxG2(wo, wi, alpha) * abs(dot(wi, wm) * dot(wo, wm) / (cosI * cosO * denom));
    return tint * (ft / (etap * etap));
}

bool SampleDielectric(vec3 wo, float alpha, float eta, vec3 u, out vec3 wi)
{
    vec3 wm = SampleGgxVisibleNormal(wo, alpha, u.xy);
    float F = FresnelDielectric(dot(wo, wm), eta);
    if (u.z < F)
    {
        wi = reflect(-wo, wm);
        return wo.z * wi.z > 0.0;
    }
    return Refract(wo, wm, eta, wi) && wo.z * wi.z < 0.0;
}

Bsdf CreateBsdf(SurfaceMaterial material, vec3 wo)
{
    Bsdf b;
    b.baseColor = material.baseColor;
    b.sheen = material.sheen;
    b.glassTint = sqrt(max(material.baseColor, vec3(0.0)));
    b.alpha = max(material.roughness * material.roughness, MIN_ALPHA);
    b.coatAlpha = max(material.clearcoatRoughness * material.clearcoatRoughness, MIN_ALPHA);
    b.ior = max(material.ior, 1.01);
    b.clearcoat = wo.z > 0.0 ? material.clearcoat : 0.0;

    float cosO = abs(wo.z);
    b.metalScale = 1.0 + material.baseColor * (1.0 / GgxAlbedo(cosO, b.alpha) - 1.0);
    b.diffuseScale = 1.0 / (1.0 - FresnelDiffuseReflectance(b.ior));
    float coatFresnel = b.clearcoat * FresnelDielectric(cosO, COAT_IOR);
    float baseWeight = 1.0 - coatFresnel;
    float dielectric = baseWeight * (1.0 - material.metallic);
    b.wMetal = baseWeight * material.metallic;
    b.wGlass = dielectric * material.transmission;
    b.wOpaque = dielectric * (1.0 - material.transmission);

    float specularFresnel = FresnelDielectric(cosO, b.ior);
    b.pCoat = coatFresnel;
    b.pMetal = b.wMetal;
    b.pGlass = b.wGlass;
    b.pSpecular = b.wOpaque * specularFresnel;
    b.pDiffuse = b.wOpaque * (1.0 - specularFresnel);
    float total = b.pCoat + b.pMetal + b.pGlass + b.pSpecular + b.pDiffuse;
    float scale = total > 0.0 ? 1.0 / total : 0.0;
    b.pCoat *= scale;
    b.pMetal *= scale;
    b.pGlass *= scale;
    b.pSpecular *= scale;
    b.pDiffuse *= scale;
    return b;
}

bool BsdfHasSmoothLobes(Bsdf b)
{
    return b.pDiffuse > 0.0 || b.alpha > NEE_MIN_ALPHA || (b.pCoat > 0.0 && b.coatAlpha > NEE_MIN_ALPHA);
}

vec3 EvalBsdf(Bsdf b, vec3 wo, vec3 wi, vec3 ng, out float pdf)
{
    pdf = 0.0;
    vec3 f = vec3(0.0);
    if (wo.z == 0.0 || wi.z == 0.0)
        return f;
    bool reflection = wo.z * wi.z > 0.0;
    if (reflection != (dot(wo, ng) * dot(wi, ng) > 0.0))
        return f;

    if (reflection)
    {
        vec3 o = vec3(wo.xy, abs(wo.z));
        vec3 i = vec3(wi.xy, abs(wi.z));
        vec3 h = normalize(o + i);
        float oh = max(dot(o, h), 1e-8);
        float denom = 4.0 * o.z * i.z;
        if (b.pCoat > 0.0)
        {
            f += vec3(b.clearcoat * FresnelDielectric(oh, COAT_IOR) * GgxD(h, b.coatAlpha) * GgxG2(o, i, b.coatAlpha) / denom);
            pdf += b.pCoat * GgxVisiblePdf(o, h, b.coatAlpha) / (4.0 * oh);
        }

        float D = GgxD(h, b.alpha);
        float G = GgxG2(o, i, b.alpha);
        float specularPdf = GgxVisiblePdf(o, h, b.alpha) / (4.0 * oh);
        if (b.wMetal > 0.0)
        {
            f += b.wMetal * b.metalScale * FresnelSchlick(b.baseColor, oh) * (D * G / denom);
            pdf += b.pMetal * specularPdf;
        }
        if (b.wOpaque > 0.0)
        {
            float fo = FresnelDielectric(o.z, b.ior);
            float fi = FresnelDielectric(i.z, b.ior);
            f += vec3(b.wOpaque * FresnelDielectric(oh, b.ior) * D * G / denom);
            f += b.wOpaque * (b.baseColor * (INV_PI * b.diffuseScale * (1.0 - fo) * (1.0 - fi)) + b.sheen * SchlickWeight(dot(i, h)));
            pdf += b.pSpecular * specularPdf + b.pDiffuse * i.z * INV_PI;
        }
    }

    if (b.wGlass > 0.0)
    {
        float glassPdf;
        f += b.wGlass * EvalDielectric(wo, wi, b.alpha, b.ior, b.glassTint, glassPdf);
        pdf += b.pGlass * glassPdf;
    }
    return f;
}

bool SampleBsdf(Bsdf b, vec3 wo, vec3 ng, inout uint rng, out vec3 wi, out vec3 f, out float pdf)
{
    float side = wo.z >= 0.0 ? 1.0 : -1.0;
    vec3 o = vec3(wo.xy, abs(wo.z));
    float lobe = Random(rng);
    vec2 u = Random2(rng);
    f = vec3(0.0);
    pdf = 0.0;

    if (lobe < b.pCoat)
    {
        vec3 i = reflect(-o, SampleGgxVisibleNormal(o, b.coatAlpha, u));
        if (i.z <= 0.0)
            return false;
        wi = vec3(i.xy, i.z * side);
    }
    else if (lobe < b.pCoat + b.pMetal + b.pSpecular)
    {
        vec3 i = reflect(-o, SampleGgxVisibleNormal(o, b.alpha, u));
        if (i.z <= 0.0)
            return false;
        wi = vec3(i.xy, i.z * side);
    }
    else if (lobe < b.pCoat + b.pMetal + b.pSpecular + b.pGlass)
    {
        if (!SampleDielectric(wo, b.alpha, b.ior, vec3(u, Random(rng)), wi))
            return false;
    }
    else
    {
        vec3 i = SampleCosineHemisphere(u);
        wi = vec3(i.xy, i.z * side);
    }

    f = EvalBsdf(b, wo, wi, ng, pdf);
    return pdf > 1e-12 && MaxComponent(f) > 0.0;
}
