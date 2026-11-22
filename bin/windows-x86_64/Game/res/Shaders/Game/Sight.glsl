const float SightChest = 0.95;
const float SightFloor = 0.45;
const float SightNear = 0.25;
const float SightGrow = 1.1;
const vec2 SightRadii = vec2(1.3, 1.65);
const float SightRagged = 0.09;
const float SightRim = 0.07;
const vec3 SightGlow = vec3(0.55, 0.36, 0.16);

float SightHash(vec3 p)
{
	p = fract(p * 0.3183099 + 0.1) * 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float SightNoise(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	vec3 u = f * f * (3.0 - 2.0 * f);
	float near = mix(mix(SightHash(i), SightHash(i + vec3(1.0, 0.0, 0.0)), u.x), mix(SightHash(i + vec3(0.0, 1.0, 0.0)), SightHash(i + vec3(1.0, 1.0, 0.0)), u.x), u.y);
	float far = mix(mix(SightHash(i + vec3(0.0, 0.0, 1.0)), SightHash(i + vec3(1.0, 0.0, 1.0)), u.x), mix(SightHash(i + vec3(0.0, 1.0, 1.0)), SightHash(i + vec3(1.0, 1.0, 1.0)), u.x), u.y);
	return mix(near, far, u.z);
}

float SightDistance(vec3 world, vec3 eye, vec4 sight)
{
	if (sight.w <= 0.0)
		return 2.0;
	float ragged = SightNoise(world * 2.3) - 0.5;
	if (world.y < sight.y + SightFloor + ragged * 0.12)
		return 2.0;
	vec3 toTarget = sight.xyz + vec3(0.0, SightChest, 0.0) - eye;
	float distance = length(toTarget);
	vec3 forward = toTarget / distance;
	vec3 offset = world - eye;
	float along = dot(offset, forward);
	float grow = smoothstep(SightNear, SightNear + SightGrow, distance - along);
	if (grow <= 0.0 || along <= 0.0)
		return 2.0;
	vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
	vec3 up = cross(forward, right);
	vec2 plane = vec2(dot(offset, right), dot(offset, up)) * (distance / along);
	return length(plane / (SightRadii * (sight.w * grow))) + ragged * SightRagged * 2.0;
}

float SightRimGlow(float distance)
{
	return 1.0 - smoothstep(1.0, 1.0 + SightRim, distance);
}
