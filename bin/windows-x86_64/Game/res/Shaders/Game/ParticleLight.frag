#version 450
#include "Particles.glsl"
#include "EffectShapes.glsl"

layout(std430, set = 0, binding = 4) readonly buffer FrameBuffer { ParticleFrame Frame; };
layout(set = 0, binding = 5) uniform sampler2D SceneDepth;

layout(location = 0) in vec2 inUv;
layout(location = 1) in vec4 inColor;
layout(location = 2) flat in uvec2 inShapeSeed;
layout(location = 3) in vec3 inLook;
layout(location = 4) flat in vec4 inParams;

layout(location = 0) out vec4 outLight;

void main()
{
	float pixel = max(length(dFdx(inUv)), length(dFdy(inUv)));
	float mask = ParticleShape(inShapeSeed.x, inUv, inShapeSeed.y, inParams, pixel, Frame.CameraRight.w);
	vec2 screen = gl_FragCoord.xy * 2.0 / Frame.Screen.xy;
	float near = Frame.CameraForward.w;
	float far = Frame.Screen.z;
	float sceneZ = near * far / (far - textureLod(SceneDepth, screen, 0.0).r * (far - near));
	float particleZ = near * far / (far - gl_FragCoord.z * (far - near));
	float fade = clamp((sceneZ - particleZ) / max(inLook.z, 0.01), 0.0, 1.0);
	float intensity = mix(clamp(inColor.a * mask, 0.0, 1.0), max(inColor.a * mask, 0.0), inLook.x) * fade;
	vec3 light = inColor.rgb * (intensity * inLook.y);
	outLight = vec4(light, dot(light, vec3(0.2126, 0.7152, 0.0722)) * particleZ);
}
