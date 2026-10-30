struct ShieldInstance
{
	vec4 Sphere;
	vec4 Forward;
	vec4 Right;
	vec4 Rise;
	vec4 Color;
	vec4 Impacts[8];
	uvec4 Info;
};

struct ShieldCell
{
	vec4 Center;
	vec4 Right;
	vec4 Up;
	vec4 Plane;
	vec4 State;
	uvec4 Info;
};

layout(std430, set = 0, binding = 0) readonly buffer FrameBuffer { GlassFrame Frame; };
layout(std430, set = 0, binding = 1) readonly buffer CellBuffer { ShieldCell Cells[]; };
layout(std430, set = 0, binding = 6) readonly buffer ShieldBuffer { ShieldInstance Shields[]; };
