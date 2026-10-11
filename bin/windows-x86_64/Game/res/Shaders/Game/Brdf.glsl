const float Pi = 3.14159265;
const float InvPi = 0.31830989;
const float MinAlpha = 0.002;
const float SurfaceIor = 1.5;

const float GgxAlbedoTable[64] = float[](
	0.9796, 0.9987, 0.9999, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
	0.9990, 0.8918, 0.9643, 0.9924, 0.9976, 0.9989, 0.9994, 0.9996,
	0.9997, 0.9490, 0.8864, 0.9164, 0.9595, 0.9803, 0.9887, 0.9924,
	0.9997, 0.9666, 0.9028, 0.8637, 0.8756, 0.9099, 0.9387, 0.9566,
	0.9997, 0.9649, 0.9007, 0.8396, 0.8051, 0.8046, 0.8265, 0.8548,
	0.9995, 0.9545, 0.8759, 0.7966, 0.7323, 0.6911, 0.6743, 0.6779,
	0.9993, 0.9390, 0.8366, 0.7330, 0.6423, 0.5692, 0.5142, 0.4758,
	0.9991, 0.9202, 0.7892, 0.6579, 0.5425, 0.4466, 0.3690, 0.3069);

float FresnelDielectric(float cosThetaI, float eta)
{
	cosThetaI = clamp(cosThetaI, 0.0, 1.0);
	float sin2ThetaT = (1.0 - cosThetaI * cosThetaI) / (eta * eta);
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

float GgxD(float cosM, float alpha)
{
	float a2 = alpha * alpha;
	float d = cosM * cosM * (a2 - 1.0) + 1.0;
	return a2 / (Pi * d * d);
}

float GgxLambda(float cosTheta, float alpha)
{
	float cos2 = max(cosTheta * cosTheta, 1e-12);
	float tan2 = max(0.0, 1.0 - cos2) / cos2;
	return 0.5 * (-1.0 + sqrt(1.0 + alpha * alpha * tan2));
}

float GgxG2(float cosO, float cosI, float alpha)
{
	return 1.0 / (1.0 + GgxLambda(cosO, alpha) + GgxLambda(cosI, alpha));
}

float GgxAlbedo(float cosTheta, float alpha)
{
	vec2 p = vec2(sqrt(clamp(cosTheta, 0.0, 1.0)), min(sqrt(alpha), 1.0)) * 7.0;
	ivec2 i = min(ivec2(p), ivec2(6));
	vec2 t = p - vec2(i);
	int row = i.y * 8 + i.x;
	float e0 = mix(GgxAlbedoTable[row], GgxAlbedoTable[row + 1], t.x);
	float e1 = mix(GgxAlbedoTable[row + 8], GgxAlbedoTable[row + 9], t.x);
	return mix(e0, e1, t.y);
}

float SurfaceAlpha(float roughness)
{
	return max(roughness * roughness, MinAlpha);
}

vec3 MetalScale(vec3 baseColor, float cosO, float alpha)
{
	return 1.0 + baseColor * (1.0 / GgxAlbedo(cosO, alpha) - 1.0);
}

vec3 EvaluateBrdf(vec3 baseColor, float metallic, float roughness, vec3 n, vec3 v, vec3 l)
{
	float cosO = dot(n, v);
	float cosI = dot(n, l);
	if (cosO <= 0.0 || cosI <= 0.0)
		return vec3(0.0);
	vec3 h = normalize(v + l);
	float cosOH = max(dot(v, h), 1e-8);
	float alpha = SurfaceAlpha(roughness);
	float specular = GgxD(dot(n, h), alpha) * GgxG2(cosO, cosI, alpha) / (4.0 * cosO * cosI);
	vec3 metal = MetalScale(baseColor, cosO, alpha) * FresnelSchlick(baseColor, cosOH) * specular;
	float diffuseScale = 1.0 / (1.0 - FresnelDiffuseReflectance(SurfaceIor));
	vec3 dielectric = vec3(FresnelDielectric(cosOH, SurfaceIor) * specular)
		+ baseColor * (InvPi * diffuseScale * (1.0 - FresnelDielectric(cosO, SurfaceIor)) * (1.0 - FresnelDielectric(cosI, SurfaceIor)));
	return mix(dielectric, metal, metallic);
}

vec2 EnvironmentBrdf(float roughness, float cosO)
{
	const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
	const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
	vec4 r = roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * cosO)) * r.x + r.y;
	return vec2(-1.04, 1.04) * a004 + r.zw;
}

vec3 HemisphereRadiance(vec3 sky, vec3 ground, float up, float sharpness)
{
	return mix(ground, sky, clamp(0.5 + 0.5 * up * sharpness, 0.0, 1.0));
}

vec3 DominantDirection(vec3 n, vec3 v, float roughness)
{
	float a = roughness * roughness;
	return normalize(mix(n, reflect(-v, n), (1.0 - a) * (sqrt(1.0 - a) + a)));
}

vec3 CombineAmbient(vec3 baseColor, float metallic, float roughness, vec3 n, vec3 v, vec3 irradiance, vec3 reflected)
{
	float cosO = max(dot(n, v), 1e-4);
	vec2 environment = EnvironmentBrdf(roughness, cosO);
	vec3 dielectric = baseColor * (1.0 - FresnelDielectric(cosO, SurfaceIor)) * irradiance + (0.04 * environment.x + environment.y) * reflected;
	if (metallic <= 0.0)
		return dielectric;
	vec3 metal = MetalScale(baseColor, cosO, SurfaceAlpha(roughness)) * (baseColor * environment.x + environment.y);
	return mix(dielectric, metal * reflected, metallic);
}

vec3 EvaluateAmbient(vec3 baseColor, float metallic, float roughness, float occlusion, vec3 n, vec3 v, vec3 sky, vec3 ground)
{
	vec3 reflected = HemisphereRadiance(sky, ground, DominantDirection(n, v, roughness).y, 1.0 / max(roughness, 0.05));
	return CombineAmbient(baseColor, metallic, roughness, n, v, HemisphereRadiance(sky, ground, n.y, 1.0), reflected) * occlusion;
}
