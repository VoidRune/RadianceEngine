#version 450
#include "Particles.glsl"
#include "Brdf.glsl"
#include "Environment.glsl"
#include "Glass.glsl"
#include "Dissolve.glsl"

layout(std430, set = 0, binding = 0) readonly buffer FrameBuffer { GlassFrame Frame; };
layout(std430, set = 0, binding = 1) readonly buffer FacetBuffer { GlassInstance Facets[]; };
layout(set = 0, binding = 2) uniform sampler2D Pyramid;
layout(set = 0, binding = 3) uniform sampler2D BackFaces;
layout(set = 0, binding = 4) uniform sampler2D SceneDepth;
layout(set = 0, binding = 5) uniform sampler2D EnvironmentMap;

layout(push_constant) uniform PushConstants
{
	uint Pass;
} pc;

layout(location = 0) in vec3 inWorld;
layout(location = 1) in vec3 inBary;
layout(location = 2) in vec2 inPlane;
layout(location = 3) flat in vec3 inNormal;
layout(location = 4) flat in uint inIndex;
layout(location = 5) flat in vec4 inGlint;
layout(location = 6) flat in vec3 inSpot;

layout(location = 0) out vec4 outScene;
layout(location = 1) out vec4 outEmission;

const float FacetDissolveScale = 6.0;
const float FacetBurnWidth = 0.1;
const float FacetStagger = 0.35;

vec3 Spectrum(float t)
{
	return clamp(abs(fract(t + vec3(0.0, 0.333, 0.667)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
}

vec2 CellPoint(vec2 cell, uint seed)
{
	uvec2 q = uvec2(ivec2(cell) + 4096);
	uint h = Hash(q.x * 73856093u ^ q.y * 19349663u ^ seed);
	return vec2(float(h & 0xFFFFu), float(h >> 16)) / 65535.0;
}

vec2 Fractures(vec2 p, uint seed)
{
	vec2 cell = floor(p);
	vec2 local = p - cell;
	vec2 nearest = vec2(0.0);
	vec2 owner = vec2(0.0);
	float best = 8.0;
	for (int y = -1; y <= 1; y++)
		for (int x = -1; x <= 1; x++)
		{
			vec2 offset = vec2(x, y);
			vec2 r = offset + CellPoint(cell + offset, seed) - local;
			float d = dot(r, r);
			if (d < best)
			{
				best = d;
				nearest = r;
				owner = offset;
			}
		}
	float edge = 8.0;
	for (int y = -2; y <= 2; y++)
		for (int x = -2; x <= 2; x++)
		{
			vec2 offset = owner + vec2(x, y);
			vec2 r = offset + CellPoint(cell + offset, seed) - local;
			vec2 gap = r - nearest;
			if (dot(gap, gap) > 1e-5)
				edge = min(edge, dot(0.5 * (nearest + r), normalize(gap)));
		}
	return vec2(edge, CellPoint(cell + owner, seed ^ 0x5BD1E995u).x);
}

float LinearDepth(float depth)
{
	float near = Frame.Forward.w;
	float far = Frame.Screen.z;
	return near * far / (far - depth * (far - near));
}

vec2 ToScreen(vec3 world)
{
	vec4 clip = Frame.ViewProjection * vec4(world, 1.0);
	return clip.xy / max(clip.w, 1e-3) * 0.5 + 0.5;
}

vec3 PyramidLevel(int level, vec2 uv)
{
	vec4 rect = Frame.Levels[level];
	vec2 inset = 0.5 * Frame.Atlas.xy;
	return textureLod(Pyramid, rect.xy + clamp(uv * rect.zw, inset, rect.zw - inset), 0.0).rgb;
}

vec3 SamplePyramid(vec2 uv, float level)
{
	uv = clamp(uv, Frame.Region.xy, Frame.Region.zw);
	level = clamp(level, 0.0, 3.0);
	float base = floor(level);
	vec3 color = PyramidLevel(int(base), uv);
	float t = level - base;
	if (t > 0.01)
		color = mix(color, PyramidLevel(int(base) + 1, uv), t);
	return color;
}

vec3 Sky(vec3 direction, float roughness)
{
	if (Frame.Environment.x > 0.0)
		return SampleEnvironment(EnvironmentMap, Frame.Environment, direction, roughness);
	float sun = max(dot(direction, Frame.SunDirection.xyz), 0.0);
	float sun5 = sun * sun;
	sun5 *= sun5 * sun;
	float sun40 = sun5 * sun5;
	sun40 *= sun40;
	sun40 *= sun40;
	return HemisphereRadiance(Frame.SkyRadiance.rgb, Frame.GroundRadiance.rgb, direction.y, 1.0 / max(roughness, 0.08))
		+ Frame.SunRadiance.rgb * (0.05 * sun5 + 0.3 * sun40);
}

float FloorHit(vec3 origin, vec3 direction, float floorY, out vec2 uv)
{
	uv = vec2(0.5);
	if (direction.y > -0.05)
		return 0.0;
	uv = ToScreen(origin + direction * max((floorY - origin.y) / direction.y, 0.0));
	vec2 inside = smoothstep(Frame.Region.xy, Frame.Region.xy + 0.03, uv) * (1.0 - smoothstep(Frame.Region.zw - 0.03, Frame.Region.zw, uv));
	return inside.x * inside.y * smoothstep(-0.05, -0.25, direction.y);
}

vec3 Glint(vec3 target, float point, float sharpness)
{
	float across = length(target.xz);
	vec2 toward = target.xz / max(across, 1e-4);
	float miss = inGlint.x * toward.y - inGlint.y * toward.x;
	vec3 offset = vec3(miss * across) + vec3(-0.008, 0.0, 0.008);
	vec3 broad = exp(-(offset * offset + inGlint.z * inGlint.z) * sharpness) * step(0.0, dot(inGlint.xy, toward));
	vec3 sharp = broad * broad * broad;
	return sharp * sharp * point + broad * 0.05;
}

void main()
{
	GlassInstance facet = Facets[inIndex];
	vec2 screen = gl_FragCoord.xy / Frame.Screen.xy;
	vec3 forward = Frame.Forward.xyz;
	vec3 ray = inWorld - Frame.Eye.xyz;
	float frontZ = dot(ray, forward);
	vec3 view = -normalize(ray);
	float sceneZ = LinearDepth(textureLod(SceneDepth, screen, 0.0).r);
	float buried = clamp((sceneZ - frontZ) / 0.06, 0.0, 1.0);
	if (buried <= 0.0)
		discard;

	float crack = facet.A.w;
	float flash = facet.B.w;
	float fade = facet.C.w * buried;
	float pane = facet.Absorb.w;
	float ior = facet.Optics.x;
	float dispersion = facet.Optics.y;
	float roughness = facet.Optics.z;
	float floorY = facet.Optics.w;
	uint seed = facet.Info.x;
	float dissolve = uintBitsToFloat(facet.Info.y);
	float burn = 0.0;
	if (dissolve > 0.0)
	{
		float field = mix(DissolveField(inWorld - inSpot, FacetDissolveScale, 0.0, float(seed & 1023u)), float(Hash(seed ^ 0x2F6Bu) & 1023u) / 1023.0, FacetStagger);
		float threshold = DissolveThreshold(dissolve, FacetBurnWidth);
		if (field <= threshold)
			discard;
		burn = DissolveEdge(field, threshold, FacetBurnWidth);
		burn *= burn;
	}
	vec3 normal = normalize(inNormal);
	vec3 tangent = normalize(cross(abs(normal.y) < 0.95 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0), normal));
	vec3 bitangent = cross(normal, tangent);
	float crackReach = length(inPlane - facet.Crack.xy);
	float crackScale = 2.4 + 3.2 * exp(-crackReach * 2.5);
	float planePixel = length(fwidth(inPlane)) * crackScale;
	vec3 pixels = inBary / max(fwidth(inBary), vec3(1e-6));
	float spotRadius = max(0.045, length(fwidth(inWorld)) * 2.0);

	float fracture = 0.0;
	float frost = 0.0;
	vec2 tilt = vec2(0.0);
#ifdef CRACKED
	if (crack > 0.01)
	{
		vec2 cells = Fractures((inPlane - facet.Crack.xy) * crackScale, seed);
		float spread = crack * 2.2 - crackReach - 0.45 * cells.y;
		float reveal = smoothstep(0.0, 0.12, spread);
		float width = 0.005 + 0.009 * crack;
		fracture = reveal * (1.0 - smoothstep(width, width + planePixel * 1.3, cells.x)) * (0.45 + 0.55 * cells.y);
		frost = reveal;
		uint h = Hash(floatBitsToUint(cells.y) ^ seed);
		tilt = (vec2(float(h & 0xFFFFu), float(h >> 16)) / 65535.0 - 0.5) * reveal;
	}
#endif
	vec3 n = normalize(normal + (tangent * tilt.x + bitangent * tilt.y) * 0.45);
	roughness = mix(roughness, 0.3, frost * 0.5);
	float level = max(roughness * 8.0 - 0.15, 0.0);

	float cosV = clamp(dot(n, view), 1e-3, 1.0);
	float fresnel = FresnelDielectric(cosV, ior);
	vec3 bounce = reflect(-view, n);
	vec3 reflected = Sky(bounce, roughness);
	vec2 floorUv;
	float floorWeight = FloorHit(inWorld, bounce, floorY, floorUv);
	if (floorWeight > 0.0)
		reflected = mix(reflected, SamplePyramid(floorUv, 1.5 + roughness * 6.0), floorWeight);

	vec4 back = texelFetch(BackFaces, ivec2(gl_FragCoord.xy), 0);
	bool solid = pane <= 0.0;
	float cosRay = max(dot(-view, forward), 0.2);
	bool exits = solid && back.w > frontZ && back.w < sceneZ;
	float inside;
	vec3 exitNormal;
	if (solid)
	{
		inside = max((back.w > frontZ ? min(back.w, sceneZ) : min(frontZ + 0.15, sceneZ)) - frontZ, 0.0) / cosRay;
		exitNormal = exits ? normalize(back.xyz) : -n;
	}
	else
	{
		float angle = float(Hash(seed * 747796405u + 1u) & 0xFFFFu) / 65535.0 * 6.2831853;
		inside = pane;
		exitNormal = normalize(-normal + (tangent * cos(angle) + bitangent * sin(angle)) * facet.Crack.z);
	}

	vec3 transmitted = vec3(0.0);
	vec3 fire = vec3(0.0);
	vec3 returnWeight = vec3(0.0);
	vec2 returnUv = vec2(0.5);
	bool returnPicked = false;
	float leak = 1.0;
	for (int c = 0; c < 3; c++)
	{
		int channel = c == 0 ? 1 : c == 1 ? 0 : 2;
		float eta = ior + (float(channel) - 1.0) * 0.5 * dispersion;
		vec3 entered = refract(-view, n, 1.0 / eta);
		vec3 exitPoint = inWorld + entered * inside;
		vec3 direction = entered;
		float beyond = 0.0;
		float exitTransmission = 1.0;
		bool returned = false;
		if (exits || !solid)
		{
			vec3 escaped = refract(entered, -exitNormal, eta);
			if (dot(escaped, escaped) > 1e-6)
			{
				direction = escaped;
				exitTransmission = 1.0 - FresnelDielectric(max(dot(entered, exitNormal), 1e-3), 1.0 / eta);
				beyond = solid ? (sceneZ - back.w) / cosRay : (sceneZ - frontZ) / cosRay - inside;
			}
			else
			{
				vec3 bounced = reflect(entered, -exitNormal);
				vec3 leaving = refract(bounced, -n, eta);
				returned = dot(bounced, n) > 0.0 && dot(leaving, leaving) > 1e-6;
				direction = returned ? leaving : bounced;
				beyond = 0.4;
			}
		}
		float value;
		if (returned)
		{
			vec2 uv;
			returnWeight[channel] = FloorHit(inWorld, direction, floorY, uv);
			if (returnWeight[channel] > 0.0 && !returnPicked)
			{
				returnUv = uv;
				returnPicked = true;
			}
			value = Sky(direction, roughness + 0.04)[channel];
			float toward = max(dot(direction, Frame.SunDirection.xyz), 0.0);
			toward *= toward * toward;
			toward *= toward;
			toward *= toward;
			toward *= toward;
			fire[channel] = toward * toward;
		}
		else
		{
			vec2 offset = ToScreen(exitPoint + direction * clamp(beyond, 0.0, 4.0)) - screen;
			float reach = length(offset * vec2(Frame.Screen.x / Frame.Screen.y, 1.0));
			offset *= min(1.0, Frame.Screen.w / max(reach, 1e-5));
			if (c == 0)
				leak = LinearDepth(textureLod(SceneDepth, screen + offset, 0.0).r) < frontZ - 0.05 ? 0.0 : 1.0;
			value = SamplePyramid(screen + offset * leak, level)[channel];
		}
		transmitted[channel] = value * exitTransmission;
	}
	if (returnPicked)
		transmitted = mix(transmitted, SamplePyramid(returnUv, 1.6 + roughness * 6.0), returnWeight);
	vec3 absorption = exp(-facet.Absorb.rgb * inside);
	vec3 scatter = exp(-facet.Absorb.rgb * 0.5) * (facet.Crack.w * (1.0 - exp(-inside * 1.2)));
	vec3 radiance = (transmitted * absorption + scatter) * (1.0 - fresnel) + reflected * fresnel;
	radiance = mix(radiance, vec3(dot(radiance, vec3(0.3333))) * 1.15 + 0.015, frost * 0.12);

	vec3 spot = inWorld - inSpot;
	float point = exp(-dot(spot, spot) / (spotRadius * spotRadius)) * 3.0;
	vec3 sunGlint = Glint(normalize(view + Frame.SunDirection.xyz), point, 100.0);
	vec3 viewGlint = Glint(view, point, 80.0);
	vec3 sparkle = (Frame.SunRadiance.rgb * (sunGlint + fire * absorption * 1.6) + viewGlint * 0.5) * facet.Edge.a;

	float rim = min(min(pixels.x, pixels.y), pixels.z);
	float edge = (1.0 - smoothstep(0.4, 1.4, rim)) * 0.03;
	float inner = exp(-min(min(inBary.x, inBary.y), inBary.z) * 14.0);
	vec3 crackLight = (vec3(1.0, 0.95, 0.9) * 0.24 + Spectrum(cosV * 2.0 + float(seed & 255u) / 255.0) * 0.1) * fracture;
	radiance += facet.Edge.rgb * (edge + flash * (0.08 + 0.2 * inner)) + crackLight + sparkle;

	vec3 burnColor = vec3(unpackHalf2x16(facet.Info.z), unpackHalf2x16(facet.Info.w).x) * burn;
	radiance += burnColor;
	outScene = vec4(radiance * fade, fade);
	vec3 glow = sparkle * 0.6 + facet.Edge.rgb * (edge * 0.5 + flash * 0.25) + crackLight * 0.2 + burnColor;
	outEmission = vec4(glow * fade, fade * 0.5);
}
