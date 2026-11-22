#version 450
#include "Grass.glsl"

layout(std430, set = 0, binding = 0) readonly buffer FrameBuffer { GrassFrame Frame; };
layout(std430, set = 0, binding = 2) readonly buffer BladeBuffer { GrassBlade Blades[]; };

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outWorld;
layout(location = 2) out vec2 outBlade;
layout(location = 3) flat out vec4 outTone;
layout(location = 4) flat out vec4 outEffects;
layout(location = 5) flat out uint outKind;

const float TwoPi = 6.2831853;
const vec2 Leaflet[6] = vec2[](vec2(0.0, 0.0), vec2(0.42, 0.48), vec2(0.92, 0.38), vec2(0.8, 0.0), vec2(0.92, -0.38), vec2(0.42, -0.48));

vec3 Bezier(vec3 a, vec3 b, vec3 c, float t)
{
	return mix(mix(a, b, t), mix(b, c, t), t);
}

void main()
{
	uint index = uint(gl_InstanceIndex);
	uint kind = index >= Frame.Draws[3].z ? GrassClover : index >= Frame.Draws[2].z ? GrassFlower : index >= Frame.Draws[1].z ? GrassFar : GrassNear;
	GrassBlade blade = Blades[index];
	vec3 base = blade.BaseHeight.xyz;
	float height = blade.BaseHeight.w;
	vec2 facingBend = unpackHalf2x16(blade.Shape.x);
	vec2 sizePhase = unpackHalf2x16(blade.Shape.y);
	vec4 effects = unpackUnorm4x8(blade.Shape.w);
	vec3 up = vec3(0.0, 1.0, 0.0);
	vec3 dir = vec3(cos(facingBend.x), 0.0, sin(facingBend.x));

	vec2 wind = Frame.Wind.xy;
	float time = Frame.Eye.w;
	float gust = GrassNoise(base.xz * 0.16 - wind * time * 0.75);
	gust = gust * gust * (3.0 - 2.0 * gust);
	float ripple = GrassNoise(base.xz * 0.9 - wind * time * 2.6 + 31.0);
	float flutter = sin(time * (2.6 + 0.8 * fract(sizePhase.y)) + sizePhase.y);
	float stiff = (1.0 - 0.85 * effects.z) * (1.0 - 0.6 * effects.x) * (1.0 - 0.6 * effects.w);
	float sway = (Frame.Wind.z * (0.35 + 0.9 * gust + 0.35 * ripple) + 0.05 * flutter) * stiff;
	vec3 lean = (kind == GrassClover ? vec3(0.0) : dir * facingBend.y) + vec3(wind.x, 0.0, wind.y) * sway;

	vec2 push = vec2(0.0);
	float flatten = 0.0;
	for (uint i = 0u; i < Frame.Counts.w; i++)
	{
		vec4 pusher = Frame.Pushers[i];
		vec2 delta = base.xz - pusher.xz;
		float dist = length(delta);
		if (dist >= pusher.w || abs(base.y - pusher.y) > 1.0)
			continue;
		float k = 1.0 - dist / pusher.w;
		k = k * k * (3.0 - 2.0 * k);
		push += (dist > 1e-3 ? delta / dist : dir.xz) * k;
		flatten = max(flatten, k);
	}
	lean += vec3(push.x, 0.0, push.y) * 1.6;

	vec3 tipDir = normalize(vec3(lean.x, 1.0, lean.z));
	vec3 p2 = base + tipDir * (height * (1.0 - 0.35 * flatten));
	vec3 p1 = base + vec3(0.0, (p2.y - base.y) * mix(0.95, 0.55, flatten), 0.0);
	vec3 across = vec3(-dir.z, 0.0, dir.x);
	uint vertex = uint(gl_VertexIndex);
	vec3 position;
	vec3 normal;
	vec2 coords;

	if (kind == GrassClover)
	{
		uint leaf = vertex / 6u;
		vec2 shape = Leaflet[vertex % 6u];
		float angle = facingBend.x + float(leaf) * TwoPi / 3.0;
		vec3 axis = vec3(cos(angle), 0.0, sin(angle));
		vec3 side = vec3(-axis.z, 0.0, axis.x);
		float tilt = facingBend.y;
		vec3 center = base + up * height + vec3(lean.x, 0.0, lean.z) * (0.6 * height);
		position = center + (axis * shape.x + side * shape.y + up * (shape.x * tilt)) * sizePhase.x;
		normal = normalize(up - axis * tilt);
		coords = vec2(3.0 + length(shape), shape.y);
	}
	else if (kind == GrassFlower && vertex >= 4u)
	{
		vec3 heading = normalize(p2 - p1 + vec3(0.0, 1e-4, 0.0));
		vec3 n = normalize(up * 1.6 + heading);
		vec3 a = normalize(cross(n, vec3(0.0, 0.0, 1.0)));
		vec3 b = cross(n, a);
		vec3 center = p2 + n * 0.005;
		position = center;
		coords = vec2(2.0, 0.0);
		if (vertex > 4u)
		{
			uint i = vertex - 5u;
			float angle = sizePhase.y + float(i) * TwoPi / 10.0;
			float r = (i & 1u) == 0u ? 1.0 : 0.45;
			float radius = r * sizePhase.x;
			position = center + (a * cos(angle) + b * sin(angle)) * radius + n * (0.25 * radius);
			coords = vec2(2.0 + r, 0.0);
		}
		normal = n;
	}
	else
	{
		bool stem = kind == GrassFlower;
		uint segments = stem ? 1u : kind == GrassFar ? Frame.Counts.y : Frame.Counts.x;
		uint segment = min(vertex / 2u, segments);
		float t = float(segment) / float(segments);
		float side = !stem && segment == segments ? 0.0 : ((vertex & 1u) == 0u ? -1.0 : 1.0);
		position = Bezier(base, p1, p2, t);
		vec3 tangent = normalize(2.0 * (1.0 - t) * (p1 - base) + 2.0 * t * (p2 - p1) + vec3(0.0, 1e-4, 0.0));
		normal = normalize(cross(tangent, across));
		vec3 view = normalize(Frame.Eye.xyz - position);
		vec3 screenSide = cross(tangent, view);
		float screenLength = length(screenSide);
		screenSide = screenLength > 1e-4 ? screenSide / screenLength : across;
		if (dot(screenSide, across) < 0.0)
			screenSide = -screenSide;
		float edgeOn = 1.0 - abs(dot(normal, view));
		vec3 widthDir = normalize(mix(across, screenSide, Frame.Look.w * edgeOn));
		float pixel = length(Frame.Eye.xyz - position) * Frame.Look.z;
		float width = stem ? max(0.01, pixel) : max(sizePhase.x * (1.0 - 0.45 * t * t), pixel * (1.0 - t));
		position += widthDir * (side * 0.5 * width);
		normal = normalize(normal + across * side * 0.55);
		coords = vec2(t, side);
	}

	outNormal = normal;
	outWorld = position;
	outBlade = coords;
	outTone = unpackUnorm4x8(blade.Shape.z);
	outEffects = effects;
	outKind = kind;
	gl_Position = Frame.ViewProjection * vec4(position, 1.0);
}
