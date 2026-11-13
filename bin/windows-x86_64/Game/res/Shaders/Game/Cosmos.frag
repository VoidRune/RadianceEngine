#version 450
#include "CosmosCommon.glsl"

layout(set = 0, binding = 1) uniform sampler2D Nebula;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

const float SlabTop = 0.42;
const float DiskRim = 1.75;
const float Cuts6[7] = float[](-1e9, -1.8, -0.75, 0.0, 0.75, 1.8, 1e9);
const float Reps6[6] = float[](-2.1, -1.12, -0.35, 0.35, 1.12, 2.1);
const float Cuts4[5] = float[](-1e9, -1.0, 0.0, 1.0, 1e9);
const float Reps4[4] = float[](-1.41, -0.44, 0.44, 1.41);

float SliceMass(float y0, float y1, float h, float span)
{
	float u0 = y0 / h;
	float u1 = y1 / h;
	float du = abs(u1 - u0);
	float u = 0.5 * (u0 + u1);
	float average = du > 0.05 ? 0.5 * abs(Erf(u1 * 0.70710678) - Erf(u0 * 0.70710678)) / du : 0.39894228 * exp(-0.5 * u * u);
	return average * span / h;
}

vec3 StarLayer(vec3 d, float cells, float pixel, float density, float seed)
{
	vec3 a = abs(d);
	vec2 uv;
	float face;
	if (a.x >= a.y && a.x >= a.z)
	{
		uv = d.yz / a.x;
		face = d.x > 0.0 ? 0.0 : 1.0;
	}
	else if (a.y >= a.z)
	{
		uv = d.xz / a.y;
		face = d.y > 0.0 ? 2.0 : 3.0;
	}
	else
	{
		uv = d.xy / a.z;
		face = d.z > 0.0 ? 4.0 : 5.0;
	}
	vec2 g = uv * cells;
	vec2 cell = floor(g);
	vec4 h = Hash43(vec3(cell, face * 37.0 + seed));
	if (h.z > density)
		return vec3(0.0);
	vec2 center = 0.25 + 0.5 * h.xy;
	float distance = length(g - cell - center) / cells / pixel / (1.0 + 0.75 * dot(uv, uv));
	float bright = 0.06 + 0.3 * h.w * h.w + 9.0 * pow(h.w, 40.0);
	float twinkle = 0.82 + 0.18 * sin(frame.Camera.w * (1.3 + 3.0 * fract(h.w * 13.7)) + h.x * 60.0);
	vec3 tone = mix(vec3(1.0, 0.78, 0.58), vec3(0.7, 0.82, 1.0), fract(h.y * 7.3));
	tone = mix(tone, vec3(1.0), 0.45);
	return tone * bright * twinkle * exp(-distance * distance * 1.6);
}

vec3 ShootingStar(vec2 uv)
{
	float period = 8.5;
	float slot = floor(frame.Camera.w / period);
	vec4 h = Hash43(vec3(slot, 7.1, 3.3));
	float progress = (frame.Camera.w - slot * period) / 0.9;
	if (progress > 1.0 || h.w < 0.35)
		return vec3(0.0);
	float aspect = frame.Screen.x * frame.Screen.w;
	vec2 start = vec2(0.15 + 0.7 * h.x, 0.08 + 0.45 * h.y);
	float angle = 0.35 + h.z * 0.8;
	vec2 direction = vec2(cos(angle), sin(angle)) * (h.x > 0.5 ? vec2(-1.0, 1.0) : vec2(1.0, 1.0));
	float span = 0.22 + 0.12 * h.w;
	vec2 head = start + direction * span * progress;
	vec2 tail = start + direction * span * max(progress - 0.45, 0.0);
	vec2 p = (uv - tail) * vec2(aspect, 1.0);
	vec2 segment = (head - tail) * vec2(aspect, 1.0);
	float along = clamp(dot(p, segment) / max(dot(segment, segment), 1e-6), 0.0, 1.0);
	float distance = length(p - segment * along) * frame.Screen.y;
	float fade = sin(3.14159 * progress);
	return vec3(0.85, 0.92, 1.0) * exp(-distance * distance * 0.6) * along * along * fade * 3.0;
}

void main()
{
	vec2 ndc = inUv * 2.0 - 1.0;
	vec2 view = (ndc - frame.Lens.xy) * vec2(frame.Right.w, frame.Up.w);
	vec3 d = normalize(frame.Forward.xyz + view.x * frame.Right.xyz - view.y * frame.Up.xyz);
	float pixel = 2.0 * frame.Up.w / frame.Screen.y;
	vec3 o = frame.Camera.xyz;
	float pattern = frame.Galaxy.x;
	bool rich = frame.Galaxy.y > 0.5;

	vec4 baked = textureLod(Nebula, OctEncode(d), 0.0);
	vec3 sky = baked.rgb * frame.Counts.w * 2.2;
	vec3 shellOrigin = o * min(1.0, 4.0 / max(length(o), 1e-3));
	float shellB = dot(shellOrigin, d);
	vec3 shell = shellOrigin + d * (-shellB + sqrt(shellB * shellB - dot(shellOrigin, shellOrigin) + 30.0));
	vec3 shellDirection = normalize(vec3(shell.z, shell.x * 0.8 + shell.y * 0.6, shell.y * 0.8 - shell.x * 0.6));
	vec3 shellColor = textureLod(Nebula, OctEncode(shellDirection), 0.0).rgb;
	sky += shellColor.gbr * vec3(0.8, 1.0, 1.15) * frame.Counts.w * 0.9;
	sky += StarLayer(d, 150.0, pixel, 0.07, 1.0);
	sky += StarLayer(d, 380.0, pixel, 0.045, 2.0) * 0.5;
	if (rich)
		sky += StarLayer(d, 800.0, pixel, 0.04, 3.0) * 0.3;

	vec3 so = vec3(o.x, o.y / BulgeFlattening, o.z);
	vec3 sd = vec3(d.x, d.y / BulgeFlattening, d.z);
	float A = dot(sd, sd);
	float B = dot(so, sd);
	float C = dot(so, so);
	float closest = max(C - B * B / A, 0.0);
	float sigma = BulgeRadius;
	float rootA = sqrt(A);
	float scale = exp(-closest / (sigma * sigma)) * sigma * 0.8862269 / rootA;
	float tCenter = -B / A;
	float g0 = Erf(rootA * (B / A) / sigma);
	vec3 bulgeColor = vec3(1.0, 0.78, 0.5);
	float core = exp(-closest / (sigma * sigma * 0.033)) * 0.14;

	float ySign = d.y >= 0.0 ? 1.0 : -1.0;
	float dy = ySign * max(abs(d.y), 1e-5);
	float tA = 0.0;
	float tB = (ySign * SlabTop - o.y) / dy;
	tA = max(tA, (-ySign * SlabTop - o.y) / dy);
	float a2 = dot(d.xz, d.xz);
	float b2 = dot(o.xz, d.xz);
	float c2 = dot(o.xz, o.xz) - DiskRim * DiskRim;
	float disc = b2 * b2 - a2 * c2;
	if (disc > 0.0 && a2 > 1e-8)
	{
		float root = sqrt(disc);
		tA = max(tA, (-b2 - root) / a2);
		tB = min(tB, (-b2 + root) / a2);
	}
	else
		tB = -1.0;

	vec3 light = vec3(0.0);
	float transmittance = 1.0;
	float coreTransmittance = 1.0;
	float tPlane = -o.y / dy;
	float frontWeight = 0.45;
	float backWeight = 0.7;
	if (tB > tA)
	{
		float tSheet = clamp(tPlane, tA, tB);
		vec3 p = o + d * tSheet;
		float rSheet = length(p.xz);
		float phi = atan(p.z, p.x);
		float yA = o.y + d.y * tA;
		float yB = o.y + d.y * tB;
		int segments = rich ? 6 : 4;
		float hPlane = StarHeight(rSheet);
		float outer = rich ? 1.8 : 1.0;
		float spread = smoothstep(0.35, 0.8, 2.0 * outer * hPlane / abs(dy) / (tB - tA));
		bool steep = spread < 0.001;
		float thin = steep ? 1.0 / abs(dy) : SliceMass(yA, yB, YoungHeight(rSheet), tB - tA);
		float arms = ArmDensity(rSheet, phi, pattern);
		vec2 polar = GalaxyPolar(rSheet, phi + pattern);
		float clumps = Noise2(polar * 9.0 + 3.0);
		if (rich)
			clumps = clumps * 0.6 + 0.4 * Noise2(polar * 26.0 + 11.0);
		float sheetEdge = smoothstep(1.7, 0.8, rSheet);
		float young = smoothstep(0.15, 0.8, arms);
		float knots = smoothstep(0.6, 1.0, arms) * smoothstep(0.78, 0.95, Noise2(polar * 30.0 + 40.0));
		vec3 oldTone = mix(vec3(1.0, 0.83, 0.62), vec3(0.62, 0.78, 1.0), young);
		vec3 sheetLight = (oldTone * exp(-rSheet / 0.65) * arms * (0.5 + 0.8 * clumps) * 0.09 + vec3(1.0, 0.35, 0.55) * knots * exp(-rSheet / 0.7) * 0.03) * thin * sheetEdge;
		float sheetTau = GalaxyDust(rSheet, phi, pattern, arms) * (steep ? 1.0 / abs(dy) : SliceMass(yA, yB, DustHeight(rSheet), tB - tA)) * 0.6;
		if (steep)
		{
			float gSheet = Erf(rootA * (tSheet - tCenter) / sigma);
			vec3 layer = oldTone * (exp(-rSheet / GalaxyScale) * 1.1 + exp(-rSheet / 0.5) * 0.12) * sheetEdge * 0.0375 / abs(dy);
			float tau = sheetTau + LaneDust(rSheet) * 0.6 / abs(dy);
			transmittance = exp(-tau);
			light += bulgeColor * scale * (gSheet - g0) * frontWeight + layer + sheetLight * (tau > 1e-3 ? (1.0 - transmittance) / tau : 1.0);
			light += transmittance * (layer + bulgeColor * scale * (1.0 - gSheet) * backWeight);
			coreTransmittance = tCenter >= tSheet ? transmittance : 1.0;
		}
		else
		{
			bool sheetDone = false;
			float previous = tA;
			float gPrevious = Erf(rootA * (tA - tCenter) / sigma);
			light += bulgeColor * scale * (gPrevious - g0) * (tA < tPlane ? frontWeight : backWeight);
			for (int k = 0; k < 6; k++)
			{
				if (k >= segments)
					break;
				float cut = rich ? Cuts6[k + 1] : Cuts4[k + 1];
				float next = k == segments - 1 ? tB : mix(clamp((ySign * cut * hPlane - o.y) / dy, tA, tB), mix(tA, tB, float(k + 1) / float(segments)), spread);
				next = max(next, previous);
				float span = next - previous;
				if (!sheetDone && tSheet < 0.5 * (previous + next))
				{
					float through = exp(-sheetTau);
					light += transmittance * sheetLight * (sheetTau > 1e-3 ? (1.0 - through) / sheetTau : 1.0);
					coreTransmittance = tCenter >= tSheet ? transmittance * through : coreTransmittance;
					transmittance *= through;
					sheetDone = true;
				}
				float gNext = Erf(rootA * (next - tCenter) / sigma);
				vec3 emission = bulgeColor * scale * (gNext - gPrevious) * (0.5 * (previous + next) < tPlane ? frontWeight : backWeight);
				float tau = 0.0;
				if (span > 1e-5)
				{
					float rep = rich ? Reps6[k] : Reps4[k];
					float tRep = clamp(mix((ySign * rep * hPlane - o.y) / dy, 0.5 * (previous + next), spread), previous, next);
					float r = length(o.xz + d.xz * tRep);
					float y0 = o.y + d.y * previous;
					float y1 = o.y + d.y * next;
					float thick = (exp(-r / GalaxyScale) * 1.1 + exp(-r / 0.5) * 0.12) * smoothstep(1.7, 0.8, r);
					emission += oldTone * thick * SliceMass(y0, y1, StarHeight(r), span) * 0.075;
					tau = LaneDust(r) * SliceMass(y0, y1, DustHeight(r), span) * 0.6;
				}
				float through = exp(-tau);
				light += transmittance * emission * (tau > 1e-3 ? (1.0 - through) / tau : 1.0);
				if (tCenter >= previous && tCenter < next)
					coreTransmittance = transmittance * exp(-tau * (tCenter - previous) / max(span, 1e-5));
				transmittance *= through;
				previous = next;
				gPrevious = gNext;
			}
			if (!sheetDone)
			{
				float through = exp(-sheetTau);
				light += transmittance * sheetLight * (sheetTau > 1e-3 ? (1.0 - through) / sheetTau : 1.0);
				transmittance *= through;
			}
			if (tCenter >= tB)
				coreTransmittance = transmittance;
			light += transmittance * bulgeColor * scale * (1.0 - gPrevious) * (tB < tPlane ? frontWeight : backWeight);
		}
	}
	else
		light += bulgeColor * scale * (1.0 - g0) * frontWeight;

	vec3 ho = vec3(o.x, o.y / 0.75, o.z);
	vec3 hd = vec3(d.x, d.y / 0.75, d.z);
	float hA = dot(hd, hd);
	float hB = dot(ho, hd);
	float haloSigma = 0.62;
	float halo = exp(-max(dot(ho, ho) - hB * hB / hA, 0.0) / (haloSigma * haloSigma)) * haloSigma * 0.8862269 / sqrt(hA) * (1.0 - Erf(sqrt(hA) * (hB / hA) / haloSigma));
	light += vec3(1.0, 0.86, 0.7) * halo * 0.035;
	light += (vec3(1.0, 0.8, 0.58) * exp(-closest / 0.09) * 0.03 + bulgeColor * core) * coreTransmittance + vec3(0.65, 0.55, 0.9) * exp(-closest / 0.6) * 0.012;
	vec3 color = sky * transmittance + light;
	color += ShootingStar(inUv);
	color *= frame.Tint.rgb;
	outColor = vec4(color, baked.a);
}
