#version 450
#include "Material.glsl"
#ifdef DISSOLVE
#include "Dissolve.glsl"
#endif

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUv;
layout(location = 2) flat in uvec3 inMaterial;
layout(location = 3) in vec4 inTint;
layout(location = 4) in vec3 inShading;
layout(location = 5) flat in vec2 inCut;
layout(location = 6) flat in uint inMirrored;
layout(location = 7) in vec4 inTangent;
layout(location = 8) in vec4 inColor;
#ifdef DISSOLVE
layout(location = 12) in vec3 inLocal;
layout(location = 13) flat in vec4 inDissolve;
layout(location = 14) flat in vec4 inBurn;
#endif

void main()
{
	Material material = materials[inMaterial.x];
#ifdef DISSOLVE
	if (DissolveField(inLocal, inDissolve.z, inDissolve.w, inBurn.w) <= DissolveThreshold(inDissolve.x, inDissolve.y))
		discard;
	if ((material.Flags & MaterialAlphaTest) == 0u)
		return;
#endif
	if (SampleAlbedo(material, inUv).a < material.Color.a)
		discard;
}
