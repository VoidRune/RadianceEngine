#version 450
#include "Particles.glsl"

layout(std430, set = 0, binding = 1) readonly buffer ParticleBuffer { Particle Particles[]; };
layout(std430, set = 0, binding = 2) readonly buffer ListBuffer { uint Lists[]; };
layout(std430, set = 0, binding = 3) readonly buffer MaterialBuffer { ParticleMaterial Materials[]; };
layout(std430, set = 0, binding = 4) readonly buffer FrameBuffer { ParticleFrame Frame; };

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
	uint capacity = Frame.Counts.w;
	uint index = Lists[capacity * (2u - Frame.Counts.z) + uint(gl_InstanceIndex)];
	Particle particle = Particles[index];
	ParticleMaterial material = Materials[particle.Info.x];
	if (pc.Layer == LightLayer ? material.Look.z <= 0.0 : uint(material.Look.y >= 0.5) != pc.Layer)
	{
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
		return;
	}
	float t = clamp(particle.PositionAge.w / particle.VelocityLife.w, 0.0, 1.0);
	vec4 color = SampleGradient(material, t);
	color.rgb *= particle.TintSize.rgb;
	float size = SampleCurve(material.Sizes, material.SizeTimes, t) * particle.TintSize.w;
	vec2 corner = Corners[gl_VertexIndex];
	vec3 center = particle.PositionAge.xyz;
	uint align = uint(material.Motion.z + 0.5);
	vec3 world;
	if (align == AlignVelocity)
	{
		vec3 velocity = particle.VelocityLife.xyz;
		vec3 axis = velocity - Frame.CameraForward.xyz * dot(velocity, Frame.CameraForward.xyz);
		float speed = length(axis);
		axis = speed > 1e-4 ? axis / speed : Frame.CameraRight.xyz;
		vec3 side = normalize(cross(Frame.CameraForward.xyz, axis));
		float stretch = size * (1.0 + material.Look.w * speed);
		world = center + axis * ((corner.x - 1.0) * stretch + size) + side * (corner.y * size);
	}
	else
	{
		float angle = uintBitsToFloat(particle.Info.w) + material.Motion.x * particle.PositionAge.w;
		vec2 rotated = vec2(corner.x * cos(angle) - corner.y * sin(angle), corner.x * sin(angle) + corner.y * cos(angle)) * size;
		if (align == AlignGround)
			world = center + vec3(rotated.x, 0.0, rotated.y);
		else
			world = center + Frame.CameraRight.xyz * rotated.x + Frame.CameraUp.xyz * rotated.y;
	}
	outUv = corner;
	outColor = color;
	outShapeSeed = uvec2(uint(material.Look.x + 0.5), particle.Info.y);
	outLook = vec3(material.Look.y, material.Look.z, material.Motion.y);
	outParams = vec4(0.0);
	gl_Position = Frame.ViewProjection * vec4(world, 1.0);
}
