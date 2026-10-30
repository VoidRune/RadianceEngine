struct DistorterInstance
{
	vec4 CenterYaw;
	vec4 Extent;
	vec4 Params;
	uvec4 Info;
};

layout(std430, set = 0, binding = 0) readonly buffer DistorterBuffer { DistorterInstance Distorters[]; };

layout(push_constant) uniform PushConstants
{
	mat4 ViewProjection;
	vec4 CameraRight;
	vec4 CameraUp;
	vec4 Screen;
} pc;

const uint DistortShockwave = 0u;
const uint DistortHaze = 1u;
const uint DistortImplosion = 2u;
const uint DistortLens = 3u;

const uint DistortBillboard = 0u;
const uint DistortGround = 1u;
