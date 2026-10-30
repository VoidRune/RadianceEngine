struct Particle
{
	vec4 PositionAge;
	vec4 VelocityLife;
	vec4 TintSize;
	uvec4 Info;
};

struct ParticleMaterial
{
	vec4 Colors[4];
	vec4 ColorTimes;
	vec4 Sizes;
	vec4 SizeTimes;
	vec4 Physics;
	vec4 Collision;
	vec4 Look;
	vec4 Motion;
};

struct ParticleSpawn
{
	vec4 Origin;
	vec4 Direction;
	vec4 Velocity;
	vec4 Ranges;
	vec4 Tint;
	vec4 Segment;
	uvec4 Info;
};

struct ParticleFrame
{
	mat4 ViewProjection;
	mat4 InverseViewProjection;
	vec4 CameraRight;
	vec4 CameraUp;
	vec4 CameraForward;
	vec4 Screen;
	uvec4 Counts;
};

struct SpriteInstance
{
	vec4 PositionSize;
	vec4 Color;
	vec4 AxisStretch;
	vec4 Params;
	vec4 Look;
	uvec4 Info;
};

struct DecalInstance
{
	vec4 CenterYaw;
	vec4 Extent;
	vec4 Color;
	vec4 Tint;
	vec4 Params;
	uvec4 Info;
};

struct ParticleState
{
	uint AliveCount;
	uint DeadCount;
	uint NextAliveCount;
	uint Padding;
	uvec4 Dispatch;
	uvec4 Draw;
};

const uint ShapeGlow = 0u;
const uint ShapeSpark = 1u;
const uint ShapeSmoke = 2u;
const uint ShapeRing = 3u;
const uint ShapeStar = 4u;
const uint ShapeShard = 5u;
const uint ShapeDebris = 6u;
const uint ShapeFlame = 7u;

const uint AlignBillboard = 0u;
const uint AlignVelocity = 1u;
const uint AlignGround = 2u;
const uint AlignPlane = 3u;

const uint LightLayer = 2u;

const uint SpawnSphere = 0u;
const uint SpawnDisc = 1u;
const uint SpawnRing = 2u;
const uint SpawnSegment = 3u;
const uint SpawnOutward = 16u;

uint Hash(uint x)
{
	x ^= x >> 16;
	x *= 0x7FEB352Du;
	x ^= x >> 15;
	x *= 0x846CA68Bu;
	x ^= x >> 16;
	return x;
}

float Random(inout uint state)
{
	state = Hash(state + 0x9E3779B9u);
	return float(state >> 8) * (1.0 / 16777216.0);
}

vec3 RandomDirection(inout uint state)
{
	float z = Random(state) * 2.0 - 1.0;
	float angle = Random(state) * 6.28318531;
	float radius = sqrt(max(1.0 - z * z, 0.0));
	return vec3(radius * cos(angle), z, radius * sin(angle));
}

vec4 SampleGradient(ParticleMaterial material, float t)
{
	vec4 times = material.ColorTimes;
	if (t <= times.x)
		return material.Colors[0];
	if (t <= times.y)
		return mix(material.Colors[0], material.Colors[1], (t - times.x) / max(times.y - times.x, 1e-4));
	if (t <= times.z)
		return mix(material.Colors[1], material.Colors[2], (t - times.y) / max(times.z - times.y, 1e-4));
	if (t <= times.w)
		return mix(material.Colors[2], material.Colors[3], (t - times.z) / max(times.w - times.z, 1e-4));
	return material.Colors[3];
}

float SampleCurve(vec4 values, vec4 times, float t)
{
	if (t <= times.x)
		return values.x;
	if (t <= times.y)
		return mix(values.x, values.y, (t - times.x) / max(times.y - times.x, 1e-4));
	if (t <= times.z)
		return mix(values.y, values.z, (t - times.y) / max(times.z - times.y, 1e-4));
	if (t <= times.w)
		return mix(values.z, values.w, (t - times.z) / max(times.w - times.z, 1e-4));
	return values.w;
}
