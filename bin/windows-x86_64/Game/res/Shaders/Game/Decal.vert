#version 450
#include "Particles.glsl"

layout(std430, set = 0, binding = 4) readonly buffer FrameBuffer { ParticleFrame Frame; };
layout(std430, set = 0, binding = 6) readonly buffer DecalBuffer { DecalInstance Decals[]; };

layout(location = 0) flat out uint outIndex;

const vec2 Corners[6] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

void main()
{
	DecalInstance decal = Decals[gl_InstanceIndex];
	vec3 center = decal.CenterYaw.xyz;
	float c = cos(decal.CenterYaw.w);
	float s = sin(decal.CenterYaw.w);
	vec2 low = vec2(1e9);
	vec2 high = vec2(-1e9);
	bool behind = false;
	for (int i = 0; i < 8; i++)
	{
		vec3 local = vec3((i & 1) != 0 ? 1.0 : -1.0, (i & 2) != 0 ? 1.0 : -1.0, (i & 4) != 0 ? 1.0 : -1.0) * vec3(decal.Extent.x, decal.Extent.z, decal.Extent.y);
		vec4 clip = Frame.ViewProjection * vec4(center + vec3(local.x * c - local.z * s, local.y, local.x * s + local.z * c), 1.0);
		if (clip.w <= 1e-3)
		{
			behind = true;
			continue;
		}
		low = min(low, clip.xy / clip.w);
		high = max(high, clip.xy / clip.w);
	}
	if (behind)
	{
		low = vec2(-1.0);
		high = vec2(1.0);
	}
	low = clamp(low, vec2(-1.0), vec2(1.0));
	high = clamp(high, vec2(-1.0), vec2(1.0));
	gl_Position = vec4(mix(low, high, Corners[gl_VertexIndex]), 0.0, 1.0);
	outIndex = uint(gl_InstanceIndex);
}
