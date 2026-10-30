#version 450
#include "Particles.glsl"
#include "Glass.glsl"

layout(std430, set = 0, binding = 0) readonly buffer FrameBuffer { GlassFrame Frame; };
layout(std430, set = 0, binding = 1) readonly buffer FacetBuffer { GlassInstance Facets[]; };

layout(push_constant) uniform PushConstants
{
	uint Pass;
} pc;

layout(location = 0) out vec3 outWorld;
layout(location = 1) out vec3 outBary;
layout(location = 2) out vec2 outPlane;
layout(location = 3) flat out vec3 outNormal;
layout(location = 4) flat out uint outIndex;
layout(location = 5) flat out vec4 outGlint;
layout(location = 6) flat out vec3 outSpot;

void main()
{
	GlassInstance facet = Facets[gl_InstanceIndex];
	vec3 a = facet.A.xyz;
	vec3 b = facet.B.xyz;
	vec3 c = facet.C.xyz;
	vec3 corner = gl_VertexIndex == 0 ? a : gl_VertexIndex == 1 ? b : c;
	vec3 normal = cross(b - a, c - a);
	float area = dot(normal, normal);
	normal = area > 1e-12 ? normal * inversesqrt(area) : vec3(0.0, 1.0, 0.0);
	float side = dot(normal, Frame.Eye.xyz - a);
	bool pane = facet.Absorb.w > 0.0;
	bool keep = area > 1e-12 && (pc.Pass == PassBack ? !pane && side < 0.0 : pane || side > 0.0);
	if (pane && side < 0.0)
		normal = -normal;
	vec3 along = b - a;
	along = dot(along, along) > 1e-12 ? normalize(along) : vec3(1.0, 0.0, 0.0);
	vec3 across = cross(normal, along);
	uint h = Hash(facet.Info.x ^ 0x9E3779B9u);
	float turn = float(h & 1023u) * (6.2831853 / 1023.0);
	vec2 heading = dot(normal.xz, normal.xz) > 1e-6 ? normalize(normal.xz) : vec2(1.0, 0.0);
	vec2 glint = vec2(heading.x * cos(turn) - heading.y * sin(turn), heading.x * sin(turn) + heading.y * cos(turn));
	uint place = Hash(h + 0x632BE5ABu);
	vec3 weights = vec3(float(place & 1023u), float((place >> 10) & 1023u), float((place >> 20) & 1023u)) / 1023.0 + 0.3;
	outWorld = corner;
	outBary = vec3(gl_VertexIndex == 0 ? 1.0 : 0.0, gl_VertexIndex == 1 ? 1.0 : 0.0, gl_VertexIndex == 2 ? 1.0 : 0.0);
	outPlane = vec2(dot(corner - a, along), dot(corner - a, across));
	outNormal = normal;
	outIndex = uint(gl_InstanceIndex);
	outGlint = vec4(glint, (float((h >> 10) & 1023u) / 1023.0 - 0.5) * 0.12, 0.0);
	outSpot = (a * weights.x + b * weights.y + c * weights.z) / dot(weights, vec3(1.0));
	gl_Position = keep ? Frame.ViewProjection * vec4(corner, 1.0) : vec4(2.0, 2.0, 0.5, 1.0);
}
