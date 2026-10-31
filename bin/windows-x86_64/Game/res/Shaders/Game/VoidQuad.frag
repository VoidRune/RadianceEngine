#version 450
#include "Void.glsl"

layout(std430, set = 0, binding = 0) readonly buffer QuadBuffer { VoidQuad Quads[]; };
layout(set = 0, binding = 1) uniform sampler2D SceneDepth;

layout(location = 0) in vec2 inUv;
layout(location = 1) flat in uint inIndex;
layout(location = 2) in vec3 inWorld;
layout(location = 3) flat in float inCenterDepth;
layout(location = 4) flat in vec3 inRight;
layout(location = 5) flat in vec3 inUp;

layout(location = 0) out vec4 outScene;
layout(location = 1) out vec4 outEmission;

float Ridge(float n, float sharpness)
{
	return pow(1.0 - abs(2.0 * n - 1.0), sharpness);
}

vec4 Singularity(VoidQuad quad, vec2 uv, float pixelUv)
{
	float radius = quad.A.x;
	float extent = quad.CenterSize.w / radius;
	vec2 p = uv * extent;
	float px = max(pixelUv * extent, 1e-4);
	float r = length(p);
	float open = quad.A.y;
	float seam = quad.A.z;
	float pupil = quad.A.w;
	float pull = quad.B.x;
	float flash = quad.B.y;
	float glow = quad.B.z;
	float point = quad.B.w;
	float tilt = quad.C.z;
	float time = pc.Eye.w;
	float seed = float(quad.Info.z % 113u);
	float angle = atan(p.y, p.x);

	float core = 1.0 - smoothstep(1.0 - px, 1.0 + px, r);
	vec3 inside = vec3(0.0);
	if (core > 0.0)
	{
		float twist = angle + 1.7 / (r + 0.35) - time * 1.4;
		inside = vec3(0.022, 0.004, 0.05) * VoidFbm(vec2(cos(twist), sin(twist)) * (1.4 + 2.2 * r) + seed) * smoothstep(0.15, 1.0, r);
		inside += vec3(0.05, 0.01, 0.13) * pow(smoothstep(0.8, 1.0, r), 3.0) * (0.5 + 0.5 * glow);

		float c = cos(tilt);
		float s = sin(tilt);
		vec2 e = mat2(c, s, -s, c) * p / 0.66;
		float pxe = px / 0.66;
		float ax = abs(e.x);
		float shape = mix(pow(max(1.0 - pow(ax, 1.7), 0.0), 0.72), max(1.0 - ax, 0.0), 0.28);
		float seamY = 0.05 * (1.0 - e.x * e.x) - 0.025 * e.x;
		float upper = seamY + 0.52 * open * shape * (1.0 + 0.14 * e.x);
		float lower = seamY - 0.4 * open * shape * (1.0 - 0.12 * e.x);
		float dUp = upper - e.y;
		float dLo = e.y - lower;
		float d = min(dUp, dLo);
		float aperture = smoothstep(-pxe, pxe, d) * (1.0 - smoothstep(1.0 - pxe, 1.0, ax)) * step(0.002, open);
		float embed = 1.0 - smoothstep(0.8, 0.95, r);

		if (aperture * embed > 0.001)
		{
			vec2 wander = 0.035 * vec2(sin(time * 1.7 + seed), cos(time * 1.3 + 2.0 * seed));
			vec2 center = quad.C.xy * vec2(0.3, 0.1) + wander * (1.0 - pull);
			float iris = 0.4 * (1.0 - 0.24 * pull);
			vec2 ip = e - center;
			float rho = length(ip);
			float phi = atan(ip.y, ip.x);
			float cells = phi / VoidTau;

			vec3 sclera = vec3(0.52, 0.47, 0.9);
			float lidShade = smoothstep(0.0, 0.17, d);
			sclera *= (0.25 + 0.75 * lidShade) * (0.35 + 0.65 * (1.0 - ax * ax));
			float wobble = VoidLoopNoise(vec2(cells * 6.0, rho * 3.0 + seed), 6.0) * 2.0;
			float vein = Ridge(VoidLoopNoise(vec2(cells * 14.0 + wobble, rho * 2.6 - seed), 14.0), 9.0);
			float twig = Ridge(VoidLoopNoise(vec2(cells * 31.0 + wobble * 1.7, rho * 6.5 + 3.0), 31.0), 13.0);
			float veins = (vein + 0.6 * twig) * smoothstep(iris, iris + 0.32, rho) * smoothstep(0.02, 0.12, d);
			float crack = Ridge(VoidNoise(e * 8.5 + 4.0 + seed), 24.0) * smoothstep(0.35, 0.85, ax);
			sclera = mix(sclera, vec3(0.09, 0.0, 0.17), clamp(veins + crack * 0.7, 0.0, 0.85));

			float t = clamp(rho / iris, 0.0, 1.0);
			float fibers = VoidLoopNoise(vec2(cells * 56.0, rho * 10.0 - time * (1.5 + 6.0 * pull)), 56.0);
			float bundles = VoidLoopNoise(vec2(cells * 23.0, rho * 4.0 + 2.0 + seed), 23.0);
			float crypts = smoothstep(0.58, 0.82, VoidLoopNoise(vec2(cells * 11.0, rho * 7.0 + 9.0), 11.0));
			vec3 irisColor = mix(vec3(0.9, 0.68, 1.7), vec3(0.34, 0.06, 1.1), smoothstep(0.12, 0.72, t));
			irisColor *= (0.55 + 0.8 * fibers * (0.55 + 0.45 * bundles)) * (1.0 - 0.45 * crypts);
			irisColor += vec3(0.9, 0.55, 1.5) * 0.6 * exp(-pow((t - 0.5 - 0.08 * (bundles - 0.5)) / 0.06, 2.0));
			irisColor = mix(irisColor, vec3(0.05, 0.0, 0.13), smoothstep(0.8, 1.0, t));
			irisColor *= 1.0 + 1.6 * flash + 0.9 * pull;
			float irisMask = 1.0 - smoothstep(iris - pxe, iris + pxe, rho);
			float corona = exp(-pow((rho - iris * 1.06) / (0.018 + pxe), 2.0)) * (0.5 + 0.5 * fibers);

			float halfHeight = iris * 0.94;
			float yy = clamp(ip.y / halfHeight, -1.0, 1.0);
			float halfWidth = iris * mix(0.05, 0.34, pupil) * pow(max(1.0 - yy * yy, 0.0), 0.62);
			float slit = abs(ip.x) - halfWidth;
			float tall = 1.0 - smoothstep(halfHeight - pxe, halfHeight + pxe, abs(ip.y));
			float slitMask = (1.0 - smoothstep(-pxe, pxe, slit)) * tall;
			float slitRim = exp(-pow(slit / (0.03 * iris + pxe), 2.0)) * tall;

			vec2 glint = e - (vec2(-0.13, 0.13) + center * 0.6);
			float spec = exp(-dot(glint, glint) / 0.0018) + 0.35 * exp(-dot(glint + vec2(-0.22, 0.2), glint + vec2(-0.22, 0.2)) / 0.0006);

			vec3 eye = sclera * (1.0 + 0.9 * flash);
			eye = mix(eye, irisColor, irisMask);
			eye += vec3(0.7, 0.45, 1.3) * corona * (1.0 - irisMask) * 0.8;
			eye *= 1.0 - slitMask * irisMask;
			eye += vec3(1.1, 0.45, 1.8) * slitRim * irisMask * (0.75 + 1.2 * flash + 0.5 * pull);
			eye += vec3(1.15, 1.05, 1.5) * spec * (1.0 - flash) * smoothstep(0.3, 0.8, open);
			eye += vec3(0.9, 0.72, 1.5) * flash * 0.55 * (1.0 - slitMask * irisMask);
			inside = mix(inside, eye, aperture * embed);
		}

		float lidGlow = clamp(open * 4.0, 0.0, 1.0) * (0.55 + 0.9 * flash + 0.6 * pull);
		float lidLine = exp(-pow(dUp / (0.028 + pxe), 2.0)) + exp(-pow(dLo / (0.028 + pxe), 2.0));
		inside += vec3(0.55, 0.15, 1.15) * lidLine * lidGlow * (1.0 - smoothstep(0.8, 1.02, ax)) * embed;
		float seamLine = exp(-pow((e.y - seamY) / (0.02 + pxe), 2.0)) * (1.0 - smoothstep(seam * 0.8, seam + 1e-3, ax));
		inside += vec3(0.85, 0.5, 1.6) * seamLine * min(seam * 4.0, 1.0) * (1.0 + flash) * embed;

		float beyond = max(-d, 0.0);
		float curl = sin(atan(e.y - seamY, e.x) * 5.0 + beyond * 24.0 - time * (3.0 + 5.0 * pull) + 3.0 * VoidNoise(e * 4.0 + seed));
		float tendril = pow(max(curl, 0.0), 10.0) * exp(-beyond * 8.0) * step(0.0, -d) * clamp(open * 2.5 + seam, 0.0, 1.0);
		inside += vec3(0.42, 0.1, 0.95) * tendril * embed * (0.45 + 0.4 * glow + 0.8 * pull);
	}

	float rimLine = exp(-pow((r - 1.0) / (0.035 + px), 2.0));
	vec3 rimColor = mix(vec3(0.3, 0.07, 0.78), vec3(1.25, 1.05, 1.75), point);
	vec3 halo = rimColor * rimLine * (0.6 + 3.5 * point) * glow;
	float lensed = 0.5 + 0.5 * smoothstep(-0.35, 0.9, p.y / max(r, 1e-3));
	float flicker = 0.5 + 0.9 * VoidLoopNoise(vec2(angle / VoidTau * 18.0 - time * 5.0, seed), 18.0);
	float photon = exp(-pow((r - 1.07 - 0.05 * lensed) / (0.02 + 0.035 * lensed + px), 2.0)) * lensed * flicker;
	halo += vec3(0.62, 0.48, 1.4) * photon * glow * (0.55 + 2.5 * point);
	halo += vec3(0.2, 0.04, 0.62) * exp(-max(r - 1.0, 0.0) * 2.6) * smoothstep(0.9, 1.05, r) * 0.16 * glow;
	halo *= 1.0 - smoothstep(0.72 * extent, extent, r);
	float shadow = smoothstep(1.0 - px, 1.0 + px, r) * exp(-max(r - 1.0, 0.0) * 3.2) * 0.7 * (1.0 - point);
	return vec4(inside * core + halo, core + shadow * (1.0 - core));
}

vec4 Disk(VoidQuad quad, vec2 uv, vec3 world, float pixelUv)
{
	float radius = quad.A.x;
	float scale = quad.CenterSize.w / radius;
	vec2 p = uv * scale;
	float px = pixelUv * scale;
	float r = length(p);
	float inner = quad.A.y / radius;
	float thin = quad.B.z;
	float outer = mix(quad.A.z / radius, inner * 1.3, thin);
	if (r < inner * 0.85 || r > outer * 1.3)
		return vec4(0.0);
	float x = (r - inner) / max(outer - inner, 1e-3);
	float theta = atan(p.y, p.x);
	float spin = quad.A.w;
	float kepler = pow(max(r / inner, 0.6), -1.5);
	float seed = float(quad.Info.z % 97u);

	const vec4 centers = vec4(0.05, 0.27, 0.5, 0.76);
	const vec4 widths = vec4(0.07, 0.1, 0.12, 0.15);
	const vec4 speeds = vec4(1.0, 0.8, 1.2, 0.55);
	const vec4 gains = vec4(1.0, 0.8, 0.65, 0.45);
	float energy = 0.0;
	for (int i = 0; i < 4; i++)
	{
		float cells = 6.0 + 3.0 * float(i);
		float u = (theta + spin * speeds[i] * kepler) / VoidTau * cells;
		float arcs = smoothstep(0.4, 0.8, VoidLoopNoise(vec2(u, seed + 7.3 * float(i) + x * 1.5), cells));
		float shift = 0.06 * (VoidLoopNoise(vec2(u * 2.0, 3.0 + float(i)), cells * 2.0) - 0.5);
		energy += arcs * gains[i] * exp(-pow((x - centers[i] - shift) / widths[i], 2.0));
	}
	float streak = pow(VoidLoopNoise(vec2((theta + spin * kepler * 1.05) / VoidTau * 48.0, x * 26.0 + seed), 48.0), 5.0) * 2.2;
	float lane = smoothstep(0.5, 0.76, VoidLoopNoise(vec2((theta + spin * kepler * 0.9) / VoidTau * 9.0, x * 5.0 + seed + 11.0), 9.0)) * quad.B.y;
	float edge = VoidLoopNoise(vec2((theta + spin * kepler * 0.7) / VoidTau * 12.0, 5.0 + seed), 12.0);
	float body = smoothstep(-0.03, 0.06, x) * (1.0 - smoothstep(0.55 + 0.3 * edge, 1.0 + 0.2 * edge, x));
	float rim = exp(-pow((r - inner) / (0.035 + px), 2.0)) * (0.55 + 0.7 * VoidLoopNoise(vec2((theta + spin * 1.1) / VoidTau * 30.0, seed), 30.0));
	float ignite = VoidLoopNoise(vec2((theta + spin * kepler) / VoidTau * 5.0, 17.0 + seed), 5.0);
	float shown = smoothstep(ignite, ignite + 0.2, quad.B.w * 1.2);

	vec3 tangent = inRight * -sin(theta) + inUp * cos(theta);
	float approach = dot(-tangent, normalize(pc.Eye.xyz - world));
	float doppler = 1.0 + 0.6 * approach;

	float e = energy * body * shown;
	vec3 color = mix(vec3(0.07, 0.01, 0.24), vec3(0.4, 0.1, 1.2), smoothstep(0.05, 0.55, e));
	color = mix(color, vec3(0.8, 0.68, 1.55), smoothstep(0.7, 1.3, e + streak * 0.4 * body));
	vec3 radiance = color * (e * 0.55 + streak * body * shown * 0.6) * doppler;
	radiance += vec3(0.75, 0.62, 1.5) * rim * shown * 0.55;
	radiance *= quad.B.x * (1.0 + 1.3 * thin) * (1.0 - 0.85 * lane);
	float alpha = clamp(lane * body * shown * 0.9 + body * shown * 0.25, 0.0, 0.94);
	return vec4(radiance + vec3(0.02, 0.0, 0.05) * alpha, alpha);
}

vec4 Shock(VoidQuad quad, vec2 uv, float pixelUv)
{
	float size = quad.CenterSize.w;
	vec2 p = uv * size;
	float px = pixelUv * size;
	float r = length(p);
	float theta = atan(p.y, p.x);
	float radius = quad.A.x;
	float width = quad.A.y;
	float intensity = quad.A.z;
	float dark = quad.A.w;
	float seed = float(quad.Info.z % 89u);
	float u = (theta + quad.B.x) / VoidTau;
	float jag = (VoidLoopNoise(vec2(u * 28.0, seed), 28.0) - 0.5) * 0.7 + (VoidLoopNoise(vec2(u * 9.0, seed + 3.0), 9.0) - 0.5) * 1.3;
	float broken = smoothstep(0.28, 0.5, VoidLoopNoise(vec2(u * 11.0, seed + 17.0), 11.0));
	float d = radius + jag * width * 0.6 - r;
	float front = exp(-pow(d / (0.05 * width + px), 2.0));
	float band = smoothstep(-0.02 * width, 0.1 * width, d) * (1.0 - smoothstep(0.15 * width, 0.6 * width, d));
	float turbulence = VoidLoopNoise(vec2(u * 36.0, d / width * 3.0 + seed), 36.0);
	float shards = smoothstep(0.86, 0.95, VoidLoopNoise(vec2(u * 90.0, d / width * 7.0 + seed * 2.0), 90.0)) * band;
	vec3 radiance = vec3(0.85, 0.7, 1.6) * front * 0.75 * broken + vec3(0.38, 0.09, 1.0) * band * turbulence * turbulence * 0.35 + vec3(1.1, 0.85, 1.7) * shards * 1.2;
	float alpha = band * dark * (0.45 + 0.55 * turbulence);
	float window = 1.0 - smoothstep(0.85 * size, size, r);
	return vec4(radiance + vec3(0.04, 0.0, 0.09) * alpha, alpha) * (intensity * window);
}

void main()
{
	VoidQuad quad = Quads[inIndex];
	float pixelUv = max(length(dFdx(inUv)), length(dFdy(inUv)));
	float selfZ = VoidLinearDepth(gl_FragCoord.z);
	vec4 result;
	if (quad.Info.x == VoidSingularity)
		result = Singularity(quad, inUv, pixelUv);
	else if (quad.Info.x == VoidDisk)
	{
		bool behind = selfZ > inCenterDepth;
		if (behind != (quad.Info.y == 1u))
			discard;
		result = Disk(quad, inUv, inWorld, pixelUv);
	}
	else
		result = Shock(quad, inUv, pixelUv);
	vec2 screen = gl_FragCoord.xy / pc.Screen.xy;
	float sceneZ = VoidLinearDepth(textureLod(SceneDepth, screen, 0.0).r);
	result *= clamp((sceneZ - selfZ) / pc.Screen.w, 0.0, 1.0);
	outScene = result;
	outEmission = vec4(result.rgb * pc.Camera.w, result.a);
}
