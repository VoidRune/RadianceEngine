#version 450
#include "Material.glsl"
#include "Lake.glsl"

layout(std430, set = 0, binding = 0) readonly buffer FrameBuffer { LakeFrame Frame; };
layout(set = 0, binding = 4) uniform sampler2D CausticMap;
layout(set = 0, binding = 5) uniform sampler2D SceneEmission;

layout(location = 0) in vec3 inWorld;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec4 inColor;
layout(location = 4) flat in uvec2 inMaterial;
layout(location = 5) flat in uint inFlags;
layout(location = 6) in vec4 inTint;
layout(location = 7) flat in vec4 inGlow;
layout(location = 8) in vec3 inLocal;
layout(location = 9) in float inPulse;
layout(location = 10) in vec4 inTangent;

layout(location = 0) out vec4 outColor;

vec3 PointGlow(vec3 n)
{
	vec3 total = vec3(0.0);
	for (uint i = 0u; i < uint(Frame.LightCount.x); i++)
	{
		vec4 place = Frame.Lights[i * 2u];
		vec3 toLight = place.xyz - inWorld;
		float d2 = dot(toLight, toLight);
		float window = max(1.0 - d2 / (place.w * place.w), 0.0);
		if (window <= 0.0)
			continue;
		float wrap = max(dot(n, toLight) * inversesqrt(max(d2, 1e-4)) * 0.6 + 0.4, 0.0);
		total += Frame.Lights[i * 2u + 1u].rgb * (window * window * wrap / (1.0 + 0.35 * d2));
	}
	return total;
}

float Lobe(vec3 n, vec3 v, vec3 l, float alpha2)
{
	vec3 h = normalize(l + v);
	float nh = max(dot(n, h), 0.0);
	float d = nh * nh * (alpha2 - 1.0) + 1.0;
	return alpha2 / (3.14159 * d * d) * max(dot(n, l), 0.0) * 0.25;
}

vec3 PointShine(vec3 n, vec3 v, float alpha2)
{
	vec3 total = vec3(0.0);
	for (uint i = 0u; i < uint(Frame.LightCount.x); i++)
	{
		vec4 place = Frame.Lights[i * 2u];
		vec3 toLight = place.xyz - inWorld;
		float d2 = dot(toLight, toLight);
		float window = max(1.0 - d2 / (place.w * place.w), 0.0);
		if (window <= 0.0)
			continue;
		total += Frame.Lights[i * 2u + 1u].rgb * (window * window / (1.0 + 0.35 * d2)) * Lobe(n, v, toLight * inversesqrt(max(d2, 1e-4)), alpha2);
	}
	return total;
}

void main()
{
	float time = Frame.Eye.w;
	vec3 n = normalize(inNormal);
	if ((inFlags & MeshIce) == 0u && inWorld.y > Frame.Plane.x + 0.01)
		discard;
	if ((inFlags & MeshIce) != 0u)
	{
		float tag = textureLod(SceneEmission, gl_FragCoord.xy / Frame.LightCount.yz, 0.0).a;
		if (tag > 0.68 && tag < 0.72)
			discard;
		float below = clamp((Frame.Plane.x - 1.15 - inWorld.y) / -1.15, 0.0, 1.0);
		outColor = vec4(mix(vec3(0.004, 0.05, 0.065), vec3(0.09, 0.56, 0.58), below * below), 1.0);
		return;
	}
	Material material = materials[inMaterial.x];
	vec2 uv = (inFlags & MeshGround) != 0u ? inWorld.xz : inUv;
	vec4 albedoSample = texture(Textures[nonuniformEXT(material.Albedo)], uv * material.Params.z);
	if ((material.Flags & MaterialAlphaTest) != 0u && albedoSample.a < material.Color.a)
		discard;
	vec3 albedo = albedoSample.rgb * material.Color.rgb;
	vec4 surface = texture(Textures[nonuniformEXT(material.Surface)], uv * material.Params.z);
	if ((inFlags & MeshLayered) != 0u && inColor.r > 0.01)
	{
		Material other = materials[inMaterial.y];
		vec3 second = texture(Textures[nonuniformEXT(other.Albedo)], inWorld.xz * other.Params.z).rgb * other.Color.rgb;
		albedo = mix(albedo, second, inColor.r);
	}
	if (material.Normal != 0u)
	{
		vec2 xy = texture(Textures[nonuniformEXT(material.Normal)], uv * material.Params.z).rg * 2.0 - 1.0;
		if ((inFlags & MeshRock) != 0u)
			xy = clamp(xy * 1.5, vec2(-0.95), vec2(0.95));
		vec3 t = inTangent.xyz - n * dot(n, inTangent.xyz);
		if (dot(t, t) > 1e-8)
		{
			t = normalize(t);
			n = normalize(t * xy.x + cross(n, t) * (xy.y * inTangent.w) + n * sqrt(max(1.0 - dot(xy, xy), 0.0)));
		}
		else
		{
			vec3 up = abs(n.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
			t = normalize(cross(up, n));
			n = normalize(n + (t * xy.x + cross(n, t) * xy.y) * 0.7);
		}
	}
	if ((inFlags & MeshVertexTint) != 0u)
		albedo *= inColor.rgb;
	if ((inFlags & MeshRock) != 0u)
		albedo *= (0.62 + 0.76 * inColor.b) * mix(vec3(0.9, 0.95, 1.06), vec3(1.08, 1.0, 0.9), inColor.a);
	albedo *= inTint.rgb;
	float depth = max(Frame.Plane.x - inWorld.y, 0.0);
	float pool = textureLod(CausticMap, vec2(inWorld.x * 0.0137 + inWorld.z * 0.0041, inWorld.z * 0.0163 - inWorld.x * 0.0037) + vec2(0.13, 0.41), 0.0).r;
	pool = Frame.Aurora.y + (1.0 - Frame.Aurora.y) * smoothstep(0.34, 0.08, pool);
	float caustic = CausticWeb(CausticMap, inWorld.xz + n.xz * 0.6, time) * Frame.Aurora.z * clamp(n.y * 1.2, 0.0, 1.0) * exp(-depth * 0.09);
	vec3 skyLight = Frame.SkyRadiance.rgb * (0.55 + 0.45 * n.y);
	vec3 moon = Frame.SunRadiance.rgb * max(dot(n, Frame.SunDirection.xyz), 0.0) * 0.2;
	vec3 water = Frame.WaterColor.rgb;
	float shelf = 1.0 + 0.55 * (1.0 - smoothstep(6.5, 10.5, depth));
	float downwell = mix(0.45, 1.0, clamp(n.y * 0.6 + 0.4, 0.0, 1.0));
	vec3 lit = albedo * ((skyLight * 0.4 + moon + water * (0.55 * downwell + caustic * caustic * 5.5)) * pool * surface.r * shelf + PointGlow(n));
	vec3 glow = inGlow.rgb + (material.Extra.rgb * surface.a) * inPulse;
	float travel = length(inWorld - vec3(Frame.Eye.x, Frame.Plane.x, Frame.Eye.z)) * 0.08 + depth;
	float fog = 1.0 - exp(-travel * Frame.WaterColor.w * (inTint.a > 0.5 ? 0.22 : 1.0));
	if (inTint.a > 0.5)
	{
		vec3 toEye = normalize(Frame.Eye.xyz - inWorld);
		float facing = clamp(dot(n, toEye), 0.0, 1.0);
		float rim = pow(1.0 - facing, 3.0);
		float rough = clamp(surface.g, 0.12, 1.0);
		float alpha2 = rough * rough * rough * rough;
		float fresnel = 0.04 + 0.96 * pow(1.0 - facing, 5.0);
		vec3 downwell = water * (0.9 + caustic * caustic * 6.0) * pool;
		vec3 shine = downwell * Lobe(n, toEye, vec3(0.0, 1.0, 0.0), alpha2) + Frame.SunRadiance.rgb * 0.25 * Lobe(n, toEye, Frame.SunDirection.xyz, alpha2) + PointShine(n, toEye, alpha2);
		lit += (shine * 4.0 + water * (rim * 0.18)) * fresnel * 6.0 * surface.r + albedo * (vec3(0.2, 0.27, 0.3) * max(n.y, 0.0) + vec3(0.03, 0.05, 0.06)) * surface.r;
	}
	float abyss = smoothstep(9.5, 16.0, depth);
	vec3 radiance = mix(lit, water * (pool * 0.13), fog) * (1.0 - 0.9 * abyss) + vec3(0.003, 0.008, 0.026) * abyss + (glow + albedo * inGlow.a) * exp(-travel * Frame.WaterColor.w * 0.3) * (1.0 - 0.5 * abyss);
	outColor = vec4(radiance, 1.0);
}
