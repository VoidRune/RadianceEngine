#version 450
#include "Sun.glsl"

layout(std430, set = 0, binding = 0) readonly buffer QuadBuffer { SunQuad Quads[]; };
layout(set = 0, binding = 1) uniform sampler2D SceneDepth;

layout(location = 0) in vec2 inUv;
layout(location = 1) flat in uint inIndex;
layout(location = 2) in vec3 inWorld;
layout(location = 3) flat in vec3 inRight;
layout(location = 4) flat in vec3 inUp;

layout(location = 0) out vec4 outScene;
layout(location = 1) out vec4 outEmission;

float SceneDistance(vec3 direction)
{
	float z = SunLinearDepth(textureLod(SceneDepth, gl_FragCoord.xy / pc.Screen.xy, 0.0).r);
	return z / max(dot(direction, pc.Forward.xyz), 1e-3);
}

float Ripple(vec3 n, vec4 beat)
{
	if (beat.w < 0.0)
		return 0.0;
	float angle = acos(clamp(dot(n, beat.xyz), -1.0, 1.0));
	float x = (angle - beat.w * 2.4) / 0.26;
	return exp(-x * x) * exp(-beat.w * 1.2) * smoothstep(0.0, 0.08, beat.w);
}

vec3 Surface(SunQuad quad, vec3 n, float mu, float time)
{
	float seed = float(quad.Info.z % 97u) * 0.37;
	vec3 s = SunSpin(quad.A.y) * n;
	vec3 base = s * 1.7 + vec3(seed, 0.0, 0.0);
	vec3 warp = vec3(SunFbm(base + vec3(0.0, 0.0, time * 0.05), 2), SunFbm(base + vec3(5.2, 1.3, time * 0.045), 2), SunFbm(base + vec3(2.1, 7.7, -time * 0.05), 2)) - 0.5;
	float magma = SunFbm(s * 3.0 + warp * 2.4 + vec3(time * 0.03, -time * 0.02, time * 0.025), 4);
	float ridge = SunFbm(s * 6.5 + warp * 3.2 + vec3(0.0, time * 0.07, 0.0), 3);
	float currents = pow(1.0 - abs(2.0 * ridge - 1.0), 5.0);
	vec3 cells = SunCells(s * 3.4 + warp * 2.2, time * 0.25);
	float edge = cells.y - cells.x;
	float ripple = Ripple(n, quad.Axis);
	float life = 0.5 + 0.5 * sin(time * (0.16 + 0.22 * fract(cells.z * 7.31)) + SunTau * cells.z);
	float crust = smoothstep(0.15, 0.65, life) * step(0.25, fract(cells.z * 3.7)) * (1.0 - 0.35 * ripple);
	float gap = mix(0.9, 0.05, crust);
	float fine = SunNoise(s * 48.0 + warp * 3.0 + vec3(time * 0.25));
	float grit = SunNoise(s * 16.0 + warp * 2.0 + vec3(seed));
	float plate = smoothstep(gap, gap + 0.14, edge) * smoothstep(0.22, 0.42, grit + 0.25 * crust);
	float dome = smoothstep(gap, gap + 0.5, edge);
	float seam = smoothstep(gap - 0.18, gap, edge) * (1.0 - plate) * crust;
	float molten = 0.4 + 0.36 * magma + 0.28 * currents + 0.06 * fine + 0.3 * seam;
	float cool = 0.14 + 0.2 * (1.0 - dome) + 0.08 * grit + 0.05 * fine;
	float temp = mix(molten, cool, plate);
	for (int i = 0; i < 3; i++)
	{
		vec4 flare = i == 0 ? quad.B : (i == 1 ? quad.C : quad.D);
		if (flare.w > 0.0)
			temp += flare.w * exp((dot(n, flare.xyz) - 1.0) * 30.0) * (0.6 + 0.4 * (1.0 - plate));
	}
	temp += 0.12 * ripple + 0.04 * quad.E.y + 0.06 * quad.E.x;
	temp -= (1.0 - mu) * 0.2;
	float intensity = (0.45 + 1.25 * temp * temp) * (0.35 + 0.65 * pow(mu, 0.5));
	return SunRamp(temp) * intensity;
}

vec3 Corona(SunQuad quad, vec3 dir, vec2 ring, float x, float time)
{
	float seed = float(quad.Info.z % 89u) * 0.53;
	float slow = time * 0.02;
	vec3 warp = vec3(SunNoise(dir * 2.0 + vec3(seed, slow, 0.0)), SunNoise(dir * 2.0 + vec3(4.0, -slow, seed)), SunNoise(dir * 2.0 + vec3(slow, 7.0, 1.0))) - 0.5;
	vec3 d = normalize(dir + warp * 0.4);
	float streak = SunFbm(d * 9.0 + vec3(0.0, 0.0, time * 0.05), 2);
	float reach = 0.35 + 1.1 * SunNoise(d * 3.0 + vec3(seed, time * 0.03, 2.0));
	float inner = exp(-x * 9.0);
	float glow = exp(-x * 3.0);
	float streamers = exp(-x * 2.2 / reach) * smoothstep(0.4, 0.8, streak);
	vec3 radiance = vec3(1.0, 0.74, 0.38) * inner * 0.9
		+ vec3(1.0, 0.5, 0.15) * glow * 0.22
		+ vec3(1.0, 0.6, 0.22) * streamers * 0.18;
	if (x < 0.55)
	{
		float lick = SunFbm(vec3(ring * 2.6, x * 2.8 - time * 0.9) + vec3(seed, 0.0, 0.0), 3);
		float tear = SunNoise(vec3(ring * 8.0, x * 5.5 - time * 1.8) + vec3(0.0, seed, 0.0));
		float body = lick * 0.8 + tear * 0.3;
		float tongues = smoothstep(0.42 + 1.5 * x, 0.6 + 1.5 * x, body);
		radiance += SunRamp(0.95 - 1.7 * x - 0.25 * (1.0 - tear)) * tongues * exp(-x * 2.5) * 1.5;
	}
	return radiance * quad.A.w;
}

void Star(SunQuad quad)
{
	float time = pc.Eye.w;
	vec3 eye = pc.Eye.xyz;
	vec3 d = normalize(inWorld - eye);
	vec3 center = quad.CenterSize.xyz;
	float seed = float(quad.Info.z % 89u) * 0.53;
	vec3 oc = eye - center;
	float b = dot(oc, d);
	float tc = -b;
	vec3 dir = normalize(eye + d * tc - center + vec3(1e-5));
	float near = sqrt(max(dot(oc, oc) - b * b, 0.0)) / quad.A.x;
	float limbBand = smoothstep(0.55, 0.8, near) * (1.0 - smoothstep(1.3, 1.5, near));
	float radius = quad.A.x;
	if (limbBand > 0.0)
	{
		vec3 spun = SunSpin(quad.A.y) * dir;
		float lump = SunFbm(spun * 3.2 + vec3(seed, time * 0.2, 0.0), 2) - 0.5;
		float boil = SunNoise(spun * 9.0 + vec3(time * 0.55, seed, 0.0)) - 0.5;
		radius *= 1.0 + (0.12 * lump + 0.04 * boil) * limbBand;
	}
	float disc = b * b - (dot(oc, oc) - radius * radius);
	float rho = sqrt(max(dot(oc, oc) - b * b, 0.0)) / radius;
	float pixel = max(tc * dot(d, pc.Forward.xyz) / pc.Camera.z / radius, 1e-4);
	float sceneT = SceneDistance(d);
	float window = 1.0 - smoothstep(0.8, 0.98, length(inUv));
	vec2 ring = inUv / max(length(inUv), 1e-4);
	vec3 corona = window > 0.0 && rho > 0.985 ? Corona(quad, dir, ring, max(rho - 1.0, 0.0), time) * window : vec3(0.0);
	corona *= smoothstep(0.985, 1.0 + pixel, rho) * clamp((sceneT - tc + 0.6) / 0.6, 0.0, 1.0);
	float coverage = 1.0 - smoothstep(1.0 - pixel, 1.0 + pixel, rho);
	vec3 surface = vec3(0.0);
	float alpha = 0.0;
	if (coverage > 0.0)
	{
		float hit = disc > 0.0 ? -b - sqrt(disc) : tc;
		vec3 n = normalize(eye + d * hit - center);
		float mu = max(dot(n, -d), 0.0);
		alpha = coverage * clamp((sceneT - hit + pc.Screen.w) / pc.Screen.w, 0.0, 1.0);
		surface = Surface(quad, n, mu, time) * quad.A.z * alpha;
	}
	vec3 radiance = surface + corona * (1.0 - alpha);
	outScene = vec4(radiance, alpha);
	outEmission = vec4((surface * 0.3 + corona * (1.0 - alpha) * 0.45) * pc.Camera.w, alpha);
}

vec3 WardGlow(SunQuad quad, vec3 n, float facing, float time)
{
	float fresnel = pow(1.0 - facing, 3.0);
	if (quad.D.x <= 0.0 && quad.D.y <= 0.0 && quad.A.w < 0.004)
		return vec3(1.0, 0.6, 0.24) * quad.A.z * fresnel;
	float veil = SunFbm(n * 3.0 + vec3(0.0, time * 0.12, time * 0.05), 2);
	float caustic = pow(1.0 - abs(2.0 * SunFbm(n * 5.0 + vec3(veil * 2.0, time * 0.15, 0.0), 2) - 1.0), 6.0);
	float glow = quad.A.z * fresnel * (0.5 + veil) + quad.A.w * (0.25 * fresnel + 0.6 * caustic * (0.3 + fresnel));
	vec3 color = vec3(1.0, 0.6, 0.24) * glow;
	for (int i = 0; i < 2; i++)
	{
		vec4 hit = i == 0 ? quad.B : quad.C;
		float power = i == 0 ? quad.D.x : quad.D.y;
		if (hit.w < 0.0 || power <= 0.0)
			continue;
		float angle = acos(clamp(dot(n, hit.xyz), -1.0, 1.0));
		float age = hit.w;
		float front = 0.08 + 1.9 * (1.0 - exp(-age * 1.6));
		float width = 0.07 + 0.12 * age;
		float x = (angle - front) / width;
		float wobble = 0.7 + 0.6 * SunNoise(n * 9.0 + vec3(time * 1.5));
		float ring = exp(-x * x) * exp(-age * 1.5) * wobble;
		float spot = exp(-angle * angle * 14.0) * exp(-age * 3.5);
		float web = caustic * exp(-angle * 2.4) * exp(-age * 1.4);
		color += power * (vec3(1.0, 0.62, 0.26) * (ring * 1.1 + web * 0.6) + vec3(1.0, 0.85, 0.6) * spot * 2.5);
	}
	return color;
}

void Ward(SunQuad quad)
{
	float time = pc.Eye.w;
	vec3 eye = pc.Eye.xyz;
	vec3 d = normalize(inWorld - eye);
	vec3 center = quad.CenterSize.xyz;
	float radius = quad.A.x;
	vec3 oc = eye - center;
	float b = dot(oc, d);
	float disc = b * b - (dot(oc, oc) - radius * radius);
	if (disc <= 0.0)
		discard;
	float root = sqrt(disc);
	float sceneT = SceneDistance(d);
	float starDisc = b * b - (dot(oc, oc) - quad.A.y * quad.A.y);
	float starT = starDisc > 0.0 ? -b - sqrt(starDisc) : 1e9;
	float limb = smoothstep(0.0, 0.35, root / radius);
	vec3 radiance = vec3(0.0);
	for (int i = 0; i < 2; i++)
	{
		float t = i == 0 ? -b - root : -b + root;
		if (t <= 0.0 || t > starT)
			continue;
		float visible = clamp((sceneT - t + 0.3) / 0.3, 0.0, 1.0);
		if (visible <= 0.0)
			continue;
		vec3 n = normalize(eye + d * t - center);
		radiance += WardGlow(quad, n, abs(dot(n, d)), time) * visible * (i == 0 ? 1.0 : 0.7);
	}
	radiance *= limb;
	outScene = vec4(radiance, 0.0);
	outEmission = vec4(radiance * pc.Camera.w * 0.6, 0.0);
}

void Haze(SunQuad quad)
{
	float time = pc.Eye.w;
	vec3 eye = pc.Eye.xyz;
	vec3 d = normalize(inWorld - eye);
	float sceneT = SceneDistance(d);
	float top = quad.A.y;
	float t0 = 0.0;
	if (eye.y > top)
	{
		if (d.y >= -1e-4)
			discard;
		t0 = (top - eye.y) / d.y;
	}
	if (sceneT <= t0)
		discard;
	vec3 source = quad.CenterSize.xyz;
	float radius = quad.A.x;
	vec3 os = source - eye;
	float tc = dot(os, d);
	float h = sqrt(max(dot(os, os) - tc * tc, 0.0) + radius * radius * 0.5);
	float scatter = (atan((sceneT - tc) / h) - atan((t0 - tc) / h)) / h;
	vec3 closest = eye + d * clamp(tc, t0, sceneT);
	vec3 q = normalize(closest - source + vec3(1e-4));
	float shafts = SunFbm(q * 6.0 + vec3(0.0, time * 0.015, 0.0), 2);
	float rays = mix(1.0, 0.25 + 1.5 * shafts * shafts, quad.C.x);
	float drift = 0.7 + 0.6 * SunNoise(closest * 0.4 + vec3(time * 0.04, time * 0.015, -time * 0.03));
	float window = 1.0 - smoothstep(0.75, 0.98, length(inUv));
	vec3 radiance = quad.B.rgb * quad.B.w * scatter * rays * drift * window;
	vec3 point = eye + d * sceneT;
	float dark = smoothstep(quad.A.z, quad.A.w, -point.y) * quad.C.y * window;
	outScene = vec4(radiance, dark);
	outEmission = vec4(radiance * pc.Camera.w * 0.15, dark);
}

void Wave(SunQuad quad)
{
	float time = pc.Eye.w;
	vec3 eye = pc.Eye.xyz;
	vec3 d = normalize(inWorld - eye);
	float seed = float(quad.Info.z % 101u);
	vec2 p = inUv * quad.CenterSize.w;
	vec3 outward = quad.Axis.xyz;
	vec2 heading = vec2(dot(outward, inRight), dot(outward, inUp));
	heading = dot(heading, heading) > 1e-6 ? normalize(heading) : vec2(0.0, 1.0);
	float r = length(p);
	float angle = acos(clamp(dot(p / max(r, 1e-4), heading), -1.0, 1.0));
	float radius = quad.A.x;
	float width = quad.A.y;
	float spread = quad.A.w;
	float theta = atan(p.y, p.x);
	float arc = 1.0 - smoothstep(spread * 0.55, spread, angle);
	float jag = SunNoise(vec3(theta * 5.0, seed, time * 0.7)) - 0.5;
	float front = exp(-pow((r - radius - jag * width * 1.2) / width, 2.0));
	float trail = smoothstep(radius * 0.3, radius, r) * step(r, radius) * 0.18;
	float filaments = 0.55 + 0.45 * SunNoise(vec3(theta * 28.0, r * 1.5 - time * 2.0, seed));
	float window = 1.0 - smoothstep(0.85, 0.99, length(inUv));
	float depth = dot(quad.CenterSize.xyz - eye, d);
	float visible = clamp((SceneDistance(d) - depth + 0.8) / 0.8, 0.0, 1.0);
	vec3 radiance = vec3(1.0, 0.68, 0.3) * (front + trail) * filaments * arc * quad.A.z * window * visible;
	outScene = vec4(radiance, 0.0);
	outEmission = vec4(radiance * pc.Camera.w, 0.0);
}

void main()
{
	SunQuad quad = Quads[inIndex];
	if (quad.Info.x == SunStarQuad)
		Star(quad);
	else if (quad.Info.x == SunHazeQuad)
		Haze(quad);
	else if (quad.Info.x == SunWardQuad)
		Ward(quad);
	else
		Wave(quad);
}
