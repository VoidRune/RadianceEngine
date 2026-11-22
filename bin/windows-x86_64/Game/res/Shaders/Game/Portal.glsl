struct PortalSheet
{
	vec4 BaseHalf;
	vec4 RightSpring;
	vec4 NormalApex;
	vec4 Params;
};

layout(push_constant) uniform PortalConstants
{
	mat4 ViewProjection;
	vec4 Eye;
	vec4 Camera;
	vec4 Screen;
} pc;

layout(std430, set = 0, binding = 0) readonly buffer SheetBuffer { PortalSheet Sheets[]; };

const float PortalTau = 6.28318531;
const float PortalMargin = 0.06;
