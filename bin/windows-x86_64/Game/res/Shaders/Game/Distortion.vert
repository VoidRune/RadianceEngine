#version 450
#include "Distortion.glsl"

layout(location = 0) out vec2 outLocal;
layout(location = 1) flat out uint outIndex;

const vec2 Corners[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

void main()
{
	DistorterInstance distorter = Distorters[gl_InstanceIndex];
	vec2 local = Corners[gl_VertexIndex] * distorter.Extent.xy;
	vec3 center = distorter.CenterYaw.xyz;
	vec3 world;
	if (distorter.Info.y == DistortGround)
	{
		float c = cos(distorter.CenterYaw.w);
		float s = sin(distorter.CenterYaw.w);
		world = center + vec3(local.x * c - local.y * s, 0.0, local.x * s + local.y * c);
	}
	else
		world = center + pc.CameraRight.xyz * local.x + pc.CameraUp.xyz * local.y;
	outLocal = local;
	outIndex = gl_InstanceIndex;
	gl_Position = pc.ViewProjection * vec4(world, 1.0);
}
