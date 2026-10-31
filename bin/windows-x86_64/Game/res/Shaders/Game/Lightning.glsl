struct LightningVertex
{
	vec4 PositionWidth;
	vec4 Glow;
	vec4 Core;
	vec4 Tangent;
};

layout(push_constant) uniform LightningConstants
{
	mat4 ViewProjection;
	vec4 Eye;
	vec4 Camera;
	vec4 Screen;
	vec4 Right;
} pc;
