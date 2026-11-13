#version 450
#include "CosmosCommon.glsl"

layout(push_constant) uniform Pass
{
	uint Kind;
	uint Count;
	uint Band;
	uint Bands;
} pass;

layout(set = 0, binding = 1) uniform sampler2D Atlas;

layout(std430, set = 0, binding = 2) readonly buffer WardenPoints
{
	uint wardenPoints[];
};

layout(location = 0) out vec2 outLocal;
layout(location = 1) out vec3 outColor;
layout(location = 2) flat out vec2 outShape;

const vec2 Corners[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0));
const float ShapeStar = 0.0;
const float ShapeSoft = 1.0;
const float ShapeBokeh = 3.0;

uint State;

uint Scramble(uint x)
{
	x ^= x >> 16;
	x *= 0x7FEB352Du;
	x ^= x >> 15;
	x *= 0x846CA68Bu;
	x ^= x >> 16;
	return x;
}

float Random()
{
	State = State * 747796405u + 2891336453u;
	uint word = ((State >> ((State >> 28u) + 4u)) ^ State) * 277803737u;
	word = (word >> 22u) ^ word;
	return float(word >> 8u) * (1.0 / 16777216.0);
}

float Gauss()
{
	return sqrt(-2.0 * log(max(Random(), 1e-5))) * cos(TwoPi * Random());
}

void Hide()
{
	gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
	outLocal = vec2(0.0);
	outColor = vec3(0.0);
	outShape = vec2(0.0);
}

bool Project(mat4 matrix, vec3 position, out vec2 pixel, out float depth)
{
	vec4 clip = matrix * vec4(position, 1.0);
	depth = clip.w;
	if (clip.w < 0.002)
		return false;
	pixel = (clip.xy / clip.w * 0.5 + 0.5) * frame.Screen.xy;
	return true;
}

void Emit(vec2 pixel, vec2 velocity, float radius, float reach, vec3 color, float shape)
{
	vec2 corner = Corners[gl_VertexIndex];
	float speed = length(velocity);
	float stretch = min(speed * 0.5, 160.0);
	vec2 axis = speed > 0.01 ? velocity / speed : vec2(1.0, 0.0);
	vec2 side = vec2(-axis.y, axis.x);
	float span = stretch / max(radius, 0.01);
	vec2 center = pixel - axis * stretch;
	vec2 local = vec2(corner.x * (span + reach), corner.y * reach);
	vec2 position = center + axis * local.x * radius + side * local.y * radius;
	gl_Position = vec4(position * frame.Screen.zw * 2.0 - 1.0, 0.5, 1.0);
	outLocal = local;
	outColor = color / (1.0 + span * 0.8);
	outShape = vec2(shape, span);
}

vec2 Swirl(vec2 pixel, float strength)
{
	if (frame.Mouse.w < 0.5)
		return pixel;
	float reach = 170.0 * frame.Forward.w;
	vec2 offset = pixel - frame.Mouse.xy;
	float weight = exp(-dot(offset, offset) / (2.0 * reach * reach));
	float angle = (0.25 + 1.3 * frame.Mouse.z) * weight * strength;
	float c = cos(angle);
	float s = sin(angle);
	offset = vec2(offset.x * c - offset.y * s, offset.x * s + offset.y * c);
	return frame.Mouse.xy + offset * (1.0 + 0.12 * weight * frame.Mouse.z * strength);
}

void Galaxy(uint index)
{
	State = Scramble(index * 3u + 1u);
	float time = frame.Camera.w;
	float pattern = frame.Galaxy.x;
	float roll = Random();
	bool bulge = roll < 0.12;
	bool onArm = roll >= 0.12 && roll < 0.58;
	float young = 0.0;
	vec3 position;
	float u1 = max(Random(), 1e-5);
	float u2 = Random();
	float gauss = sqrt(-2.0 * log(u1)) * cos(TwoPi * u2);
	float gaussOther = sqrt(-2.0 * log(u1)) * sin(TwoPi * u2);
	if (bulge)
	{
		float a = BulgeRadius * sqrt(-log(max(Random(), 1e-5))) * 0.9;
		float z = Random() * 2.0 - 1.0;
		float t = Random() * TwoPi + time * -0.09;
		position = vec3(sqrt(1.0 - z * z) * cos(t), z * BulgeFlattening, sqrt(1.0 - z * z) * sin(t)) * a;
	}
	else if (onArm)
	{
		float r = 0.17 + 1.3 * pow(Random(), 1.15);
		float width = ArmWidth(r);
		float arm = Random() < 0.5 ? 0.0 : 1.0;
		if (Random() < 0.32)
			width *= 2.3;
		float angle = ArmAngle(r, arm) - pattern + gauss * width / r;
		r += gaussOther * width * 0.45;
		young = step(Random(), 0.7);
		float height = Gauss() * (young > 0.5 ? YoungHeight(r) : 0.03 + 0.03 * r);
		position = vec3(cos(angle) * r, height, sin(angle) * r);
	}
	else
	{
		float r = -GalaxyScale * log(max(Random() * Random(), 1e-6));
		if (r > 1.5)
			r = 0.3 + 1.2 * Random();
		float angle = Random() * TwoPi - 0.012 / (r + 0.07) * time;
		float height = gauss * (Random() < 0.1 ? 0.12 : 0.022 + 0.03 * r + 0.035 * exp(-r / 0.15));
		position = vec3(cos(angle) * r, height, sin(angle) * r);
	}
	float r = length(position.xz);
	float arms = onArm ? 1.0 : ArmDensity(r, atan(position.z, position.x), pattern);
	float luminosity = 0.25 + 0.75 * Random();
	float rare = Random();
	luminosity *= 1.0 + 8.0 * pow(rare, 400.0);
	vec3 color = mix(vec3(1.0, 0.78, 0.56), vec3(1.0, 0.93, 0.84), Random());
	color = mix(color, vec3(0.64, 0.79, 1.0), young);
	luminosity *= 1.0 + 1.6 * young;
	if (bulge)
		color = vec3(1.0, 0.8, 0.56);
	float edge = smoothstep(1.6, 1.15, r);
	float knotRoll = Random();
	vec2 pixel;
	vec2 previous;
	float depth;
	float previousDepth;
	if (!Project(frame.ViewProjection, position, pixel, depth) || !Project(frame.PreviousViewProjection, position, previous, previousDepth))
	{
		Hide();
		return;
	}
	float focal = 0.5 * frame.Screen.y / frame.Up.w;
	float exposure = frame.Lens.z;
	if (onArm && knotRoll < 0.012)
	{
		float glow = edge * smoothstep(0.22, 0.4, r) * 0.7;
		float radius = clamp((0.002 + 0.005 * Random()) * focal / depth, 1.0, 30.0);
		vec3 tone = Random() < 0.6 ? vec3(1.0, 0.32, 0.55) : vec3(0.35, 0.8, 1.0);
		Emit(pixel, vec2(0.0), radius, 1.8, tone * glow * 0.06 * exposure * min(400.0 / (radius * radius), 1.0), ShapeSoft);
		return;
	}
	vec3 toCamera = normalize(frame.Camera.xyz - position);
	float behind = 0.5 - 0.5 * Erf(position.y / DustHeight(r) * 0.70710678 * (toCamera.y >= 0.0 ? 1.0 : -1.0));
	vec3 crossing = position + toCamera * clamp(-position.y / (toCamera.y >= 0.0 ? max(toCamera.y, 0.01) : min(toCamera.y, -0.01)), 0.0, 1.2);
	float crossingR = length(crossing.xz);
	float shade = exp(-DiskDust(crossingR, atan(crossing.z, crossing.x), pattern, arms) * 2.6 * behind * min(1.0 / max(abs(toCamera.y), 0.01), 8.0));
	float radius = 0.7 + 0.22 * sqrt(luminosity);
	float intensity = luminosity * 0.17 * exposure / (0.25 + depth * depth) * edge * shade;
	Emit(pixel, pixel - previous, radius, 2.6, color * intensity, ShapeStar);
}

void Dust(uint index)
{
	State = Scramble(index * 3u + 2u);
	const float Cell = 0.75;
	float time = frame.Camera.w;
	vec3 base = vec3(Random(), Random(), Random()) * Cell;
	vec3 phase = vec3(Random(), Random(), Random()) * TwoPi;
	vec3 drift = vec3(sin(time * 0.05 + phase.x), sin(time * 0.043 + phase.y) + time * 0.03, sin(time * 0.037 + phase.z)) * 0.04;
	vec3 camera = frame.Camera.xyz;
	vec3 position = camera + mod(base + drift - camera, Cell) - 0.5 * Cell;
	vec3 previousCamera = camera;
	float distance = length(position - camera);
	float fade = smoothstep(Cell * 0.5, Cell * 0.32, distance) * smoothstep(0.015, 0.06, distance);
	vec2 pixel;
	vec2 previous;
	float depth;
	float previousDepth;
	if (fade < 0.01 || !Project(frame.ViewProjection, position, pixel, depth) || !Project(frame.PreviousViewProjection, position, previous, previousDepth))
	{
		Hide();
		return;
	}
	float focal = 0.5 * frame.Screen.y / frame.Up.w;
	float size = 0.0006 + 0.0012 * Random();
	float radius = size * focal / depth;
	float tone = Random();
	vec3 color = tone < 0.5 ? vec3(1.0, 0.9, 0.78) : (tone < 0.8 ? vec3(0.6, 0.85, 1.0) : vec3(1.0, 0.55, 0.75));
	float near = smoothstep(4.0, 14.0, radius);
	float twinkle = 0.6 + 0.4 * sin(time * (0.7 + 1.6 * Random()) + phase.x * 3.0);
	float swirlDepth = clamp(0.5 / depth, 0.3, 1.6) * mix(1.0, 0.12, smoothstep(2.0, 5.0, radius));
	vec2 swirled = Swirl(pixel, swirlDepth);
	vec2 previousSwirled = Swirl(previous, swirlDepth);
	radius = clamp(radius, 0.9, 26.0);
	float energy = fade * twinkle * 0.42 * (1.0 - 0.5 * frame.Warden[6].x) * (1.0 + 2.0 * pow(Random(), 6.0));
	vec3 light = color * energy * mix(1.0, 1.6 / (radius * radius) + 0.0015, near);
	Emit(swirled, (swirled - previousSwirled) * (1.0 - near) * 0.35 * (1.0 - frame.Warden[6].x), radius, near > 0.5 ? 1.15 : 2.6, light, near > 0.5 ? ShapeBokeh : ShapeStar);
}

void Title(uint index)
{
	State = Scramble(index * 3u + 3u);
	uint count = uint(frame.Title.z);
	if (count == 0u)
	{
		Hide();
		return;
	}
	float pick = Random() * frame.Title.w;
	uint glyph = 0u;
	for (uint i = 0u; i < count; i++)
	{
		glyph = i;
		if (glyphs[i].Area.x >= pick)
			break;
	}
	TitleGlyph g = glyphs[glyph];
	vec2 target = vec2(-1e5);
	for (int attempt = 0; attempt < 12; attempt++)
	{
		vec2 u = vec2(Random(), Random());
		vec3 field = textureLod(Atlas, mix(g.Uv.xy, g.Uv.zw, u), 0.0).rgb;
		float inside = max(min(field.r, field.g), min(max(field.r, field.g), field.b));
		if (inside > 0.56)
		{
			target = g.Rect.xy + (u * 2.0 - 1.0) * g.Rect.zw;
			break;
		}
	}
	if (target.x < -1e4)
	{
		Hide();
		return;
	}
	float scale = frame.Forward.w;
	float time = frame.Title.x;
	float start = 1.6 + 0.13 * float(glyph) + 1.4 * Random();
	float appear = smoothstep(start, start + 0.8, time) * frame.Title.y;
	float phase = Random() * TwoPi;
	float rate = 0.35 + 0.9 * Random();
	float glint = pow(0.5 + 0.5 * sin(frame.Camera.w * rate + phase), 10.0);
	float warmth = 0.0;
	if (frame.Mouse.w > 0.5)
	{
		vec2 fromMouse = target - frame.Mouse.xy;
		float reach = 110.0 * scale;
		warmth = exp(-dot(fromMouse, fromMouse) / (2.0 * reach * reach));
	}
	float tone = Random();
	vec3 color = tone < 0.7 ? vec3(1.0, 0.95, 0.86) : vec3(0.7, 0.86, 1.0);
	color = mix(color, vec3(1.0, 0.74, 0.42), warmth * 0.85);
	float radius = (1.0 + 0.8 * Random()) * max(scale, 0.8);
	vec3 light = color * appear * (0.15 + 2.2 * glint + 0.8 * warmth * glint);
	if (max(light.r, max(light.g, light.b)) < 0.004)
	{
		Hide();
		return;
	}
	Emit(target, vec2(0.0), radius, 2.6, light, ShapeStar);
}

vec3 Bezier(vec3 a, vec3 b, vec3 c, vec3 d, float t)
{
	float u = 1.0 - t;
	return a * (u * u * u) + b * (3.0 * u * u * t) + c * (3.0 * u * t * t) + d * (t * t * t);
}

vec3 WardenTurn(vec3 v)
{
	vec2 turn = frame.Warden[1].xy;
	float roll = frame.Warden[4].w;
	v = vec3(v.x * cos(roll) - v.y * sin(roll), v.x * sin(roll) + v.y * cos(roll), v.z);
	float lean = frame.Warden[3].w;
	v = vec3(v.x, v.y * cos(lean) + v.z * sin(lean), -v.y * sin(lean) + v.z * cos(lean));
	return vec3(v.x * turn.x + v.z * turn.y, v.y, -v.x * turn.y + v.z * turn.x);
}

vec3 WardenSkin(uint bone, vec4 p)
{
	return vec3(dot(frame.WardenBones[bone * 3u], p), dot(frame.WardenBones[bone * 3u + 1u], p), dot(frame.WardenBones[bone * 3u + 2u], p));
}

const float WardenGain = 0.3;

void Warden(uint index)
{
	vec4 state = frame.Warden[1];
	float shown = state.z;
	if (shown < 0.002 || index >= uint(frame.Warden[2].w))
	{
		Hide();
		return;
	}
	State = Scramble(index * 7u + 0x51ED27u);
	float time = frame.Camera.w;
	vec4 place = frame.Warden[0];
	vec2 pixel;
	vec2 previous;
	float depth;
	float previousDepth;
	if (index < 4u)
	{
		vec3 eye = place.xyz + WardenTurn(frame.Warden[4 + (index & 1u)].xyz) * place.w;
		if (!Project(frame.ViewProjection, eye, pixel, depth) || !Project(frame.PreviousViewProjection, eye, previous, previousDepth))
		{
			Hide();
			return;
		}
		float glint = smoothstep(0.55, 0.9, state.w) * (0.8 + 0.2 * sin(time * 0.7 + float(index & 1u))) * max(shown, 0.85) * frame.Lens.z;
		if (index < 2u)
			Emit(pixel, vec2(0.0), 3.0, 2.6, vec3(0.9, 0.8, 1.0) * 9.0 * glint, ShapeStar);
		else
			Emit(pixel, vec2(0.0), 16.0, 1.8, vec3(0.62, 0.4, 1.0) * 0.3 * glint, ShapeSoft);
		return;
	}
	uint a = wardenPoints[index * 3u];
	uint b = wardenPoints[index * 3u + 1u];
	uint c = wardenPoints[index * 3u + 2u];
	vec3 rest = frame.Warden[2].xyz + vec3(float(a & 0xFFFFu), float(a >> 16u), float(b & 0xFFFFu)) / 65535.0 * frame.Warden[3].xyz;
	vec3 restNormal = OctDecode(vec2(float((b >> 16u) & 0xFFu), float(b >> 24u)) / 255.0);
	uint bone0 = (c >> 8u) & 0xFFu;
	uint bone1 = (c >> 16u) & 0xFFu;
	float weight = float(c >> 24u) / 255.0;
	if (bone0 == bone1 && (bone0 == 3u || bone0 == 6u) && weight > 0.0)
	{
		uint side = bone0 == 3u ? 0u : 1u;
		vec4 pivot = frame.Warden[7u + side * 2u];
		vec3 axis = frame.Warden[8u + side * 2u].xyz;
		float angle = pivot.w * weight;
		vec3 offset = rest - pivot.xyz;
		rest = pivot.xyz + offset * cos(angle) + cross(axis, offset) * sin(angle) + axis * dot(axis, offset) * (1.0 - cos(angle));
		restNormal = restNormal * cos(angle) + cross(axis, restNormal) * sin(angle) + axis * dot(axis, restNormal) * (1.0 - cos(angle));
		weight = 1.0;
	}
	vec3 local = mix(WardenSkin(bone1, vec4(rest, 1.0)), WardenSkin(bone0, vec4(rest, 1.0)), weight);
	vec3 normal = normalize(mix(WardenSkin(bone1, vec4(restNormal, 0.0)), WardenSkin(bone0, vec4(restNormal, 0.0)), weight));
	float edge = float(c & 0xFFu) / 255.0;
	vec3 wander = vec3(sin(time * 0.31 + Random() * TwoPi), sin(time * 0.23 + Random() * TwoPi), sin(time * 0.27 + Random() * TwoPi)) * 0.0018;
	wander -= normal * dot(wander, normal);
	float life = fract(Random() + time * (0.04 + 0.06 * Random()));
	float spark = step(0.9, Random()) * edge;
	vec3 world = place.xyz + WardenTurn(local + wander + (normal * 0.025 + vec3(0.0, 0.012, 0.0)) * spark * life) * place.w;
	vec3 worldNormal = WardenTurn(normal);
	float delay = 0.55 * Random();
	float travel = smoothstep(delay, delay + 0.45, state.w);
	if (travel < 1.0)
	{
		float radius = 0.3 + 1.2 * sqrt(Random());
		float angle = Random() * TwoPi + time * 0.02;
		vec3 start = vec3(cos(angle) * radius, Gauss() * 0.04, sin(angle) * radius);
		vec3 path = world - start;
		world = Bezier(start, mix(start, world, 0.33) + vec3(-path.z, 0.0, path.x) * 0.25, world + worldNormal * 0.8, world, travel);
	}
	if (!Project(frame.ViewProjection, world, pixel, depth) || !Project(frame.PreviousViewProjection, world, previous, previousDepth))
	{
		Hide();
		return;
	}
	float facing = dot(worldNormal, normalize(frame.Camera.xyz - world));
	vec3 toGalaxy = world - dot(world, normalize(frame.Camera.xyz)) * normalize(frame.Camera.xyz);
	float clear = WardenGain * (bone0 == 3u || bone0 == 6u ? 1.0 : 1.0 - 0.7 * smoothstep(1.3, 0.3, length(toGalaxy)));
	float rim = 1.0 - abs(facing);
	float hue = Noise3(rest * 9.0 + 4.0);
	vec3 fill = mix(vec3(0.14, 0.24, 1.0), vec3(0.5, 0.22, 1.0), smoothstep(0.35, 0.7, hue));
	vec3 glow = mix(vec3(0.4, 0.68, 1.0), vec3(0.88, 0.94, 1.0), edge * edge);
	float edgeLight = edge * edge * edge;
	vec3 color = fill * 0.25 + glow * (edgeLight * (bone0 == 3u || bone0 == 6u ? 0.55 : 0.95) + rim * rim * rim * 0.6);
	if (index % 23u == 0u)
	{
		float size = 0.012 * place.w * 0.5 * frame.Screen.y / frame.Up.w / depth;
		float radius = clamp(size, 2.0, 60.0);
		Emit(pixel, vec2(0.0), radius, 1.8, fill * (0.5 + hue) * 0.008 * clear * 0.6 * min(size * size / (radius * radius), 1.0) * frame.Warden[6].y * shown * travel * frame.Lens.z, ShapeSoft);
		return;
	}
	float twinkle = 0.75 + 0.25 * sin(time * (0.6 + 2.0 * Random()) + Random() * TwoPi);
	float intensity = 0.085 * clear * frame.Warden[6].y * min(40.0 / (depth * depth), 2.0) * (facing > 0.0 ? 1.0 : 0.4) * twinkle * shown * (0.2 + 0.8 * travel) * (1.0 - spark * life) * frame.Lens.z;
	Emit(pixel, pixel - previous, 0.75 + 0.3 * edgeLight, 2.6, color * intensity, ShapeStar);
}

void main()
{
	uint index = gl_InstanceIndex;
	if (pass.Kind == 0u)
		Galaxy(index);
	else if (pass.Kind == 1u)
		Dust(index);
	else if (pass.Kind == 2u)
		Title(index);
	else
		Warden(index);
}
