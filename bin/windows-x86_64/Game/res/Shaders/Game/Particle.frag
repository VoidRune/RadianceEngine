#version 450
#include "Particles.glsl"
#include "EffectShapes.glsl"
#include "Fire.glsl"

layout(std430, set = 0, binding = 4) readonly buffer FrameBuffer { ParticleFrame Frame; };
layout(set = 0, binding = 5) uniform sampler2D SceneDepth;

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec4 inColor;
layout(location = 2) flat in uvec2 inShapeSeed;
layout(location = 3) in vec3 inLook;
layout(location = 4) flat in vec4 inParams;

layout(location = 0) out vec4 outScene;
layout(location = 1) out vec4 outEmission;

const vec2 NearFade = vec2(0.4, 1.5);

void main()
{
	float pixel = max(length(dFdx(inUv)), length(dFdy(inUv)));
	uint shape = inShapeSeed.x;
	float mask;
	vec3 tone = inColor.rgb;
	float hot = 1.0;
	if (shape == ShapeFire || shape == ShapeFlameSheet || shape == ShapeFlameWall)
	{
		vec2 fire = FireField(shape, inUv, inShapeSeed.y, inParams, Frame.CameraRight.w);
		mask = fire.x;
		tone = FireRamp(inColor.rgb, fire.y);
		hot = FireHot(fire.y);
	}
	else
		mask = ParticleShape(shape, inUv, inShapeSeed.y, inParams, pixel, Frame.CameraRight.w);
	vec2 screen = gl_FragCoord.xy / Frame.Screen.xy;
	float near = Frame.CameraForward.w;
	float far = Frame.Screen.z;
	float sceneZ = near * far / (far - textureLod(SceneDepth, screen, 0.0).r * (far - near));
	float particleZ = near * far / (far - gl_FragCoord.z * (far - near));
	float fade = clamp((sceneZ - particleZ) / max(inLook.z, 0.01), 0.0, 1.0) * smoothstep(NearFade.x, NearFade.y, particleZ);
	float coverage = clamp(inColor.a * mask, 0.0, 1.0) * fade;
	float intensity = mix(coverage, max(inColor.a * mask, 0.0) * fade, inLook.x);
	vec3 radiance = tone * intensity;
	outScene = vec4(radiance, coverage * (1.0 - inLook.x));
	outEmission = vec4(radiance * (inLook.y * 2.0 * hot), 0.0);
}
