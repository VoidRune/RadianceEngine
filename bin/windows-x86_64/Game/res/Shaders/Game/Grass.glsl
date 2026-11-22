struct GrassBlade
{
	vec4 BaseHeight;
	uvec4 Shape;
};

const uint GrassNear = 0u;
const uint GrassFar = 1u;
const uint GrassFlower = 2u;
const uint GrassClover = 3u;
const uint GrassKinds = 4u;
const uint GrassMaxStamps = 96u;

const uint StampBurn = 0u;
const uint StampFrost = 1u;
const uint StampFlatten = 2u;
const uint StampTrail = 3u;
const uint StampWave = 4u;

struct GrassFrame
{
	mat4 ViewProjection;
	vec4 Eye;
	ivec4 Region;
	uvec4 Draws[4];
	uvec4 Counts;
	vec4 Grid;
	vec4 Field;
	vec4 FieldSize;
	vec4 Effects;
	vec4 EffectSize;
	vec4 Wind;
	vec4 Shape;
	vec4 Root;
	vec4 Tip;
	vec4 Dry;
	vec4 Look;
	vec4 Decay;
	vec4 Pushers[32];
	vec4 Stamps[288];
};

uint GrassHash(uint x)
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

uint GrassHash2(ivec2 p, uint salt)
{
	return GrassHash(uint(p.x) * 0x8da6b343u ^ uint(p.y) * 0xd8163841u ^ salt * 0xcb1ab31fu);
}

float GrassUnit(uint h)
{
	return float(h >> 8) * (1.0 / 16777216.0);
}

float GrassNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = p - i;
	vec2 u = f * f * (3.0 - 2.0 * f);
	ivec2 c = ivec2(i);
	float a = GrassUnit(GrassHash2(c, 71u));
	float b = GrassUnit(GrassHash2(c + ivec2(1, 0), 71u));
	float d = GrassUnit(GrassHash2(c + ivec2(0, 1), 71u));
	float e = GrassUnit(GrassHash2(c + ivec2(1, 1), 71u));
	return mix(mix(a, b, u.x), mix(d, e, u.x), u.y);
}
