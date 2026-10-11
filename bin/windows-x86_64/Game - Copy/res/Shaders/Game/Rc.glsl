layout(std140, set = 0, binding = 0) uniform CascadeFrame
{
	mat4 View;
	mat4 InverseView;
	vec4 Projection;
	vec4 Screen;
	vec4 Sky;
	vec4 Ground;
	vec4 Trace;
	vec4 Shading;
	uvec4 Options;
	vec4 SolidAngles[4];
	vec4 LevelOffsets[3];
} Rc;

const int LevelCount = 6;

float LinearDepth(float depth)
{
	return Rc.Projection.z * Rc.Projection.w / (Rc.Projection.w - depth * (Rc.Projection.w - Rc.Projection.z));
}

bool IsBackground(float z)
{
	return z >= Rc.Projection.w * 0.98;
}

vec3 ViewPosition(vec2 uv, float z)
{
	return vec3((uv * 2.0 - 1.0) * z / Rc.Projection.xy, z);
}

vec2 ProjectUv(vec3 p)
{
	return p.xy * Rc.Projection.xy / p.z * 0.5 + 0.5;
}

vec2 LevelOffset(int level)
{
	vec4 offsets = Rc.LevelOffsets[level >> 1];
	return (level & 1) == 0 ? offsets.xy : offsets.zw;
}

ivec2 LevelSize(int level)
{
	return ivec2(ceil(Rc.Screen.xy / float(2 << level))) + 1;
}

ivec2 LevelTexel(vec2 uv, int level)
{
	float block = float(2 << level);
	return clamp(ivec2(floor((uv * Rc.Screen.xy - LevelOffset(level)) / block)) + 1, ivec2(0), LevelSize(level) - 1);
}

vec2 LevelUv(vec2 uv, int level)
{
	return ((uv * Rc.Screen.xy - LevelOffset(level)) / float(2 << level) + 1.0) / vec2(LevelSize(level));
}

vec2 TexelUv(ivec2 texel)
{
	return (vec2(texel) * 2.0 + LevelOffset(0) - 1.0) / Rc.Screen.xy;
}

vec2 HalfCoord(vec2 pixel)
{
	return (pixel - LevelOffset(0)) * 0.5 + 0.5;
}

float HalfTexelWorld(float z)
{
	return 4.0 * z / (abs(Rc.Projection.y) * Rc.Screen.y);
}

ivec2 ProbeTexel(ivec2 probe, float spacing, vec2 offset)
{
	return clamp(ivec2(vec2(probe) * spacing + offset - 0.5 * spacing), ivec2(0), ivec2(Rc.Screen.zw) - 1);
}

vec2 ProbeGrid(vec2 texel, float spacing, vec2 offset)
{
	return (texel - offset + 0.5 * spacing) / spacing;
}

vec3 TileDirection(ivec2 texel, int directions)
{
	int rows = directions / 2;
	bool lower = texel.y >= rows;
	vec2 ab = vec2((float(texel.x) + 0.5) / float(directions), (float(texel.y - (lower ? rows : 0)) + 0.5) / float(rows)) * 2.0 - 1.0;
	vec2 xz = vec2(ab.x + ab.y, ab.x - ab.y) * 0.5;
	float y = 1.0 - abs(xz.x) - abs(xz.y);
	return normalize(vec3(xz.x, lower ? -y : y, xz.y));
}

vec2 TileCoordinate(vec3 direction, int directions)
{
	vec3 n = direction / (abs(direction.x) + abs(direction.y) + abs(direction.z));
	float rows = float(directions / 2);
	vec2 coordinate = clamp(vec2((n.x + n.z + 1.0) * 0.5 * float(directions), (n.x - n.z + 1.0) * 0.5 * rows), vec2(0.5), vec2(float(directions), rows) - 0.5);
	return direction.y < 0.0 ? coordinate + vec2(0.0, rows) : coordinate;
}

float PlaneWeight(vec3 p, vec3 origin, vec3 normal, float tolerance)
{
	float d = dot(p - origin, normal) / tolerance;
	return max(1.0 - d * d, 0.0);
}

vec3 EnvironmentRadiance(vec3 direction)
{
	return HemisphereRadiance(Rc.Sky.rgb, Rc.Ground.rgb, direction.y, 8.0);
}
