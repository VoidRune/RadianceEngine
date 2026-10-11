#version 450
#include "Particles.glsl"

layout(std430, set = 0, binding = 4) readonly buffer FrameBuffer { ParticleFrame Frame; };
layout(std430, set = 0, binding = 6) readonly buffer SpriteBuffer { SpriteInstance Sprites[]; };

layout(push_constant) uniform PushConstants
{
	uint Layer;
} pc;

layout(location = 0) out vec2 outUv;
layout(location = 1) out vec4 outColor;
layout(location = 2) flat out uvec2 outShapeSeed;
layout(location = 3) out vec3 outLook;
layout(location = 4) flat out vec4 outParams;

const vec2 Corners[6] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

void main()
{
	SpriteInstance sprite = Sprites[gl_InstanceIndex];
	if (pc.Layer == LightLayer && sprite.Look.y <= 0.0)
	{
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
		return;
	}
	vec2 corner = Corners[gl_VertexIndex];
	vec3 center = sprite.PositionSize.xyz;
	float size = sprite.PositionSize.w;
	vec3 world;
	if (sprite.Info.y == AlignVelocity)
	{
		vec3 axis = sprite.AxisStretch.xyz;
		float length2 = dot(axis, axis);
		axis = length2 > 1e-8 ? axis * inversesqrt(length2) : Frame.CameraRight.xyz;
		vec3 side = cross(Frame.CameraForward.xyz, axis);
		float side2 = dot(side, side);
		side = side2 > 1e-8 ? side * inversesqrt(side2) : Frame.CameraUp.xyz;
		world = center + axis * (corner.x * (size + sprite.AxisStretch.w)) + side * (corner.y * size);
	}
	else
	{
		float c = cos(sprite.Look.w);
		float s = sin(sprite.Look.w);
		vec2 rotated = vec2(corner.x * c - corner.y * s, corner.x * s + corner.y * c) * size;
		if (sprite.Info.y == AlignGround)
			world = center + vec3(rotated.x, 0.0, rotated.y);
		else
			world = center + Frame.CameraRight.xyz * rotated.x + Frame.CameraUp.xyz * rotated.y;
	}
	outUv = corner;
	outColor = sprite.Color;
	outShapeSeed = uvec2(sprite.Info.x, sprite.Info.z);
	outLook = sprite.Look.xyz;
	outParams = sprite.Params;
	gl_Position = Frame.ViewProjection * vec4(world, 1.0);
}
