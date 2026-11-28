struct GlassFrame
{
	mat4 ViewProjection;
	vec4 Eye;
	vec4 Forward;
	vec4 Screen;
	vec4 Region;
	vec4 Levels[4];
	vec4 Atlas;
	vec4 SunDirection;
	vec4 SunRadiance;
	vec4 SkyRadiance;
	vec4 GroundRadiance;
	vec4 Environment;
	vec4 Translucent;
	vec4 Lights[32];
	vec4 Sheen;
	vec4 SheenRadiance;
	vec4 Fish[12];
	vec4 Ripples[8];
	vec4 WaterCounts;
};

struct GlassInstance
{
	vec4 A;
	vec4 B;
	vec4 C;
	vec4 Absorb;
	vec4 Edge;
	vec4 Optics;
	vec4 Crack;
	uvec4 Info;
};

const uint PassBack = 0u;
const uint PassFront = 1u;
