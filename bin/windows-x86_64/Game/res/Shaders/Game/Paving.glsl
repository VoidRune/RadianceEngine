const float PavingCellRange = 0.25;
const float PavingDepth = 0.065;
const float PavingFloor = 0.12;
const float PavingSpan = 0.42;

float PavingLevel(float height, float joint)
{
	return clamp((height - joint + PavingFloor) / PavingSpan, 0.0, 1.0);
}

vec2 PavingDrift(vec3 eye, vec3 position)
{
	vec3 view = normalize(eye - position);
	return -view.xz / max(view.y, 0.3) * PavingDepth;
}

float PavingNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = p - i;
	f = f * f * (3.0 - 2.0 * f);
	vec4 h = fract(sin(vec4(dot(i, vec2(127.1, 311.7)), dot(i + vec2(1.0, 0.0), vec2(127.1, 311.7)), dot(i + vec2(0.0, 1.0), vec2(127.1, 311.7)), dot(i + vec2(1.0, 1.0), vec2(127.1, 311.7)))) * 43758.547);
	return mix(mix(h.x, h.y, f.x), mix(h.z, h.w, f.x), f.y);
}

vec2 PavingCentre(vec2 world, vec2 cells, float scale)
{
	return world + (cells - 0.5) * (2.0 * PavingCellRange) / scale;
}

float PavingThreshold(vec2 centre)
{
	return 0.5 + 0.4 * (PavingNoise(centre * 1.3 + vec2(4.0, -2.0)) - 0.5);
}
