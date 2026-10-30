#version 450
#include "Particles.glsl"
#include "Glass.glsl"
#include "HexShield.glsl"

layout(set = 0, binding = 2) uniform sampler2D Pyramid;
layout(set = 0, binding = 4) uniform sampler2D SceneDepth;

layout(location = 0) in vec3 inWorld;
layout(location = 1) in vec2 inLocal;
layout(location = 2) in vec2 inPlane;
layout(location = 3) in vec3 inNormal;
layout(location = 4) flat in uint inIndex;

layout(location = 0) out vec4 outScene;
layout(location = 1) out vec4 outEmission;

const float Apothem = 0.8660254;

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
	float base = floor(level);
	return mix(PyramidLevel(int(base), uv), PyramidLevel(int(base) + 1, uv), level - base);
}

float Unit(uint h)
{
	return float(h & 0xFFFFu) / 65535.0;
}

vec3 Cracks(vec2 p, uint seed)
{
	vec2 cell = floor(p);
	vec2 local = p - cell;
	float first = 8.0;
	float second = 8.0;
	uint owner = 0u;
	for (int y = -1; y <= 1; y++)
		for (int x = -1; x <= 1; x++)
		{
			uvec2 q = uvec2(ivec2(cell) + ivec2(x, y) + 64);
			uint h = Hash(q.x * 73856093u ^ q.y * 19349663u ^ seed);
			vec2 r = vec2(x, y) + vec2(Unit(h), Unit(h >> 16)) * 0.8 + 0.1 - local;
			float d = dot(r, r);
			if (d < first)
			{
				second = first;
				first = d;
				owner = h;
			}
			else if (d < second)
				second = d;
		}
	return vec3(sqrt(second) - sqrt(first), Unit(Hash(owner)), Unit(Hash(owner + 1u)));
}

void main()
{
	ShieldCell cell = Cells[inIndex];
	ShieldInstance shield = Shields[cell.Info.x];
	bool attached = cell.Info.z != 0u;
	vec2 screen = gl_FragCoord.xy / Frame.Screen.xy;
	vec3 ray = inWorld - Frame.Eye.xyz;
	float frontZ = dot(ray, Frame.Forward.xyz);
	float sceneZ = LinearDepth(textureLod(SceneDepth, screen, 0.0).r);
	float gap = sceneZ - frontZ;
	if (gap <= 0.0)
		discard;

	float time = Frame.Eye.w;
	float form = cell.Up.w;
	float flare = cell.Plane.z;
	float crack = cell.Plane.w;
	float flicker = cell.State.x;
	float fade = cell.State.y;
	float lattice = uintBitsToFloat(shield.Info.z);
	float shatter = uintBitsToFloat(shield.Info.y);
	vec3 tint = shield.Color.rgb;
	uint seed = cell.Info.y;

	vec3 view = -normalize(ray);
	vec3 normal = normalize(inNormal);
	if (dot(normal, view) < 0.0)
		normal = -normal;
	float cosV = clamp(abs(dot(normal, view)), 0.0, 1.0);

	vec2 q = abs(inLocal);
	float border = Apothem - max(q.y, dot(q, vec2(Apothem, 0.5)));
	float aa = max(fwidth(border), 1e-4);
	float line = 1.0 - smoothstep(0.055, 0.055 + aa * 1.5, border);
	float halo = exp(-border * 7.0);

	float sector = floor(mod(atan(inLocal.y, inLocal.x), 6.2831853) / 1.0471976);
	float facing = 0.5235988 + sector * 1.0471976;
	vec2 across = vec2(cos(facing), sin(facing));
	vec2 along = vec2(-across.y, across.x);
	along = along.x < -0.01 ? -along : along;
	vec2 middle = cell.Plane.xy + across * (Apothem * lattice);
	uint edge = Hash(uint(int(round(middle.x * 64.0)) + 4096) * 73856093u ^ uint(int(round(middle.y * 64.0)) + 4096) * 19349663u ^ shield.Info.x);
	float scroll = fract(dot(inPlane, along) * 1.6 - time * (0.9 + 0.6 * Unit(edge >> 8)) + Unit(edge));
	float streak = scroll * scroll;
	streak *= streak;
	streak *= streak * ((edge & 1u) == 0u ? 1.0 : 0.45);
	float breathe = 0.5 + 0.5 * sin(length(inPlane) * 6.0 - time * 4.2);

	float ripple = 0.0;
	vec2 push = vec2(0.0);
	if (attached)
		for (int k = 0; k < 8; k++)
		{
			vec4 impact = shield.Impacts[k];
			if (impact.z >= 1.1)
				continue;
			vec2 offset = inPlane - impact.xy;
			float reach = length(offset);
			float front = (reach - impact.z * 2.6) / 0.2;
			float wave = impact.w * exp(-front * front - impact.z * 2.4);
			ripple += wave;
			push += offset / max(reach, 1e-3) * (wave * -front);
		}
	ripple = min(ripple, 1.1);

	float fracture = 0.0;
	float mottle = 1.0;
	vec2 tilt = vec2(0.0);
	if (crack > 0.01)
	{
		vec3 shards = Cracks(inLocal * 1.05 + vec2(Unit(seed), Unit(seed >> 16)) * 9.0, seed);
		float reveal = smoothstep(0.0, 0.2, crack * 1.4 - shards.y);
		float width = 0.012 + 0.022 * crack;
		fracture = reveal * (1.0 - smoothstep(width, width + fwidth(shards.x) * 1.5, shards.x));
		mottle = mix(1.0, 0.55 + 0.9 * shards.z, crack);
		tilt = (shards.yz - 0.5) * crack;
	}

	float build = smoothstep(0.0, 0.45, form);
	float fill = smoothstep(0.35, 1.0, form);
	float snap = form * (1.0 - form) * 4.0;

	vec2 bulge = inLocal * (0.22 * fill) + push * 0.6 + tilt * 0.9;
	vec3 bent = normalize(normal + cell.Right.xyz * bulge.x + cell.Up.xyz * bulge.y);
	vec3 straight = -view;
	vec3 refracted = refract(straight, bent, 1.0 / 1.25);
	float beyond = clamp(gap, 0.2, 3.0);
	vec2 offset = ToScreen(inWorld + refracted * beyond) - ToScreen(inWorld + straight * beyond);
	float reach = length(offset * vec2(Frame.Screen.x / Frame.Screen.y, 1.0));
	offset *= min(1.0, Frame.Screen.w / max(reach, 1e-5));
	if (LinearDepth(textureLod(SceneDepth, screen + offset, 0.0).r) < frontZ - 0.05)
		offset = vec2(0.0);
	float level = 0.6 + 0.8 * crack;
	vec3 behind = vec3(SamplePyramid(screen + offset * 1.18, level).r, SamplePyramid(screen + offset, level).g, SamplePyramid(screen + offset * 0.82, level).b);

	float fresnel = 1.0 - cosV;
	fresnel *= fresnel * fresnel;
	float contact = attached ? exp(-gap / 0.05) : 0.0;
	float settle = min(gap / 0.025, 1.0);

	float lit = line * build * (0.45 + 0.25 * breathe + 1.8 * streak)
		+ halo * fill * 0.16 * mottle
		+ fresnel * fill * 0.3
		+ contact * 1.6
		+ ripple * (0.3 + 1.4 * line)
		+ flare * (0.5 + 1.0 * line + 0.6 * halo)
		+ snap * (0.08 + 0.35 * line)
		+ shatter * (0.1 + 0.3 * line);
	lit *= flicker;
	vec3 white = vec3(0.85, 0.97, 1.0);
	vec3 glow = tint * lit
		+ white * (fracture * 0.3 * flicker + line * 0.3 * (flare + ripple) + contact * 0.35 + snap * line * 0.1 + shatter * line * 0.15) + tint * (fracture * 0.25);
	glow *= shield.Color.w;

	float coverage = clamp((0.16 * mottle + 0.2 * fresnel + 0.25 * halo + 0.3 * line) * fill + 0.2 * line * build, 0.0, 0.8) * fade * settle;
	vec3 through = behind * mix(vec3(1.0), tint * 0.9 + 0.12, 0.35 + 0.2 * crack);
	float shine = fade * settle;
	outScene = vec4(through * coverage + glow * shine, coverage);
	outEmission = vec4(glow * (0.45 * shine), coverage * 0.5);
}
