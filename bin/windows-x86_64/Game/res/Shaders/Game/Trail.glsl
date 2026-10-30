struct TrailPoint
{
	vec4 PositionWidth;
	vec4 Color;
	vec4 Tangent;
	vec4 Extra;
};

struct TrailStyle
{
	vec4 Core;
	vec4 Look;
	vec4 Pattern;
};

layout(push_constant) uniform TrailConstants
{
	mat4 ViewProjection;
	vec4 Eye;
	vec4 Camera;
	vec4 Screen;
	vec4 Right;
} pc;

const uint TrailFlame = 0u;
const uint TrailFrost = 1u;
const uint TrailObsidian = 2u;
const uint TrailChain = 3u;
const uint TrailSmoke = 4u;
const uint TrailWisp = 5u;
const uint TrailSwipe = 6u;
