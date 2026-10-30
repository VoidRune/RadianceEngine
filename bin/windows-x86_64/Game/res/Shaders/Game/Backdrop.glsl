const float CloudHigh = -24.0;
const float CloudLow = -58.0;
const float AbyssHeight = -380.0;
const float CalderaDepth = -240.0;
const vec2 CalderaBelow = vec2(24.0, 170.0);

float BackdropHash(vec2 p)
{
	vec3 q = fract(vec3(p.xyx) * 0.1031);
	q += dot(q, q.yzx + 33.33);
	return fract((q.x + q.y) * q.z);
}

float BackdropNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(BackdropHash(i), BackdropHash(i + vec2(1.0, 0.0)), u.x), mix(BackdropHash(i + vec2(0.0, 1.0)), BackdropHash(i + vec2(1.0, 1.0)), u.x), u.y);
}

float BackdropFbm(vec2 p)
{
	const mat2 turn = mat2(1.6, 1.2, -1.2, 1.6);
	float sum = 0.0;
	float amplitude = 0.5;
	for (int octave = 0; octave < 4; octave++)
	{
		sum += amplitude * BackdropNoise(p);
		p = turn * p + vec2(17.3, -9.1);
		amplitude *= 0.5;
	}
	return sum / 0.9375;
}

vec2 PlaneHit(vec3 origin, vec3 direction, float height)
{
	return origin.xz + direction.xz * ((origin.y - height) / max(-direction.y, 1e-3));
}

float TaperedCapsule(vec2 p, vec2 a, vec2 b, float ra, float rb)
{
	p -= a;
	b -= a;
	float h = dot(b, b);
	vec2 q = vec2(dot(p, vec2(b.y, -b.x)), dot(p, b)) / h;
	q.x = abs(q.x);
	float taper = ra - rb;
	vec2 c = vec2(sqrt(h - taper * taper), taper);
	float k = c.x * q.y - c.y * q.x;
	if (k < 0.0)
		return sqrt(h * dot(q, q)) - ra;
	if (k > c.x)
		return sqrt(h * (dot(q, q) + 1.0 - 2.0 * q.y)) - rb;
	return dot(c, q) - ra;
}

const float LeviathanDepth = -40.0;
const float LeviathanLength = 26.0;
const float LeviathanTravel = 170.0;

vec4 Leviathan(vec3 origin, vec3 direction, float time, float progress)
{
	vec2 p = PlaneHit(origin, direction, LeviathanDepth);
	float arc = 3.14159 * progress;
	vec2 head = vec2(-60.0 + LeviathanTravel * progress, 58.0 + 8.0 * sin(arc));
	vec2 heading = normalize(vec2(LeviathanTravel, 8.0 * 3.14159 * cos(arc)));
	vec2 side = vec2(-heading.y, heading.x);
	vec2 q = p - head;
	float s = -dot(q, heading);
	float lateral = dot(q, side);
	if (s < -3.0 || s > LeviathanLength + 6.0 || abs(lateral) > 13.0)
		return vec4(0.0);
	float L = LeviathanLength;
	float d = lateral - 1.4 * sin(s * 0.26 - time * 1.3) * smoothstep(2.0, L, s);
	float body = 2.3 * smoothstep(-0.6, 3.5, s) * (1.0 - 0.82 * smoothstep(0.3 * L, 0.97 * L, s));
	float bodyMask = (1.0 - smoothstep(body - 0.6, body + 0.6, abs(d))) * smoothstep(-0.8, 0.4, s) * (1.0 - smoothstep(L - 0.4, L + 0.6, s));

	float flap = 0.82 + 0.18 * sin(time * 0.9);
	vec2 local = vec2(abs(d), s);
	float wing = TaperedCapsule(local, vec2(1.6, 0.22 * L), vec2(9.0 * flap, 0.22 * L + 6.0), 1.9, 0.3);
	float rear = TaperedCapsule(local, vec2(0.6, 0.6 * L), vec2(3.4 * flap, 0.6 * L + 2.6), 0.8, 0.2);
	float fluke = TaperedCapsule(local, vec2(0.0, L - 0.6), vec2(3.6, L + 2.6), 0.75, 0.25);
	float limbs = min(wing, min(rear, fluke));
	float mask = max(bodyMask, 1.0 - smoothstep(-0.45, 0.45, limbs));

	vec3 color = vec3(0.006, 0.004, 0.014) + vec3(0.12, 0.045, 0.07) * (4.0 * mask * (1.0 - mask));
	float cell = fract(s / 1.7) - 0.5;
	float row = abs(abs(d) - 0.55 * body);
	float spot = smoothstep(0.32, 0.08, length(vec2(cell * 1.7, row))) * step(1.6, s) * step(s, L - 3.0);
	float wave = 0.5 + 0.5 * sin(s * 0.5 - time * 2.2);
	color += vec3(0.22, 0.5, 1.0) * spot * (0.5 + 1.5 * wave) * bodyMask;
	float eye = smoothstep(0.3, 0.05, length(vec2(s - 1.3, abs(d) - 0.85)));
	color += vec3(0.7, 0.85, 1.0) * 2.5 * eye;
	float fade = smoothstep(0.0, 0.08, progress) * (1.0 - smoothstep(0.92, 1.0, progress));
	return vec4(color, mask * fade * 0.8);
}

const float CloudSea = -95.0;
const float CloudWisps = -30.0;

vec3 DaySky(vec3 origin, vec3 direction, float time, vec3 toLight)
{
	float down = max(-direction.y, 1e-3);
	vec3 color = mix(vec3(0.5, 0.66, 0.88), vec3(0.25, 0.44, 0.76), smoothstep(0.15, 0.9, down));

	vec2 sea = PlaneHit(origin, direction, CloudSea);
	vec2 flow = vec2(0.0035, 0.0012) * time;
	float shape = BackdropFbm(sea * 0.011 + flow);
	vec2 sunward = normalize(toLight.xz + vec2(1e-4));
	float toward = BackdropFbm((sea + sunward * 16.0) * 0.011 + flow);
	float shade = clamp(0.62 + (shape - toward) * 2.4, 0.38, 1.0);
	vec3 cloud = mix(vec3(0.56, 0.65, 0.8), vec3(1.0, 0.98, 0.94), smoothstep(0.5, 0.95, shape)) * shade;
	color = mix(color, cloud, smoothstep(0.34, 0.7, shape) * 0.96);

	vec2 wisp = PlaneHit(origin, direction, CloudWisps);
	float streak = BackdropFbm(vec2(wisp.x * 0.03, wisp.y * 0.045) + vec2(time * 0.012, 0.0));
	color = mix(color, vec3(0.92, 0.95, 1.0), smoothstep(0.62, 0.9, streak) * 0.3);

	return mix(color, vec3(0.72, 0.82, 0.94), pow(1.0 - down, 3.0) * 0.55);
}

const float StormSea = -58.0;
const float StormDeck = -13.0;

float DeckDensity(vec2 p, float time)
{
	vec2 flow = vec2(0.014, 0.006) * time;
	float shape = BackdropFbm(p * 0.032 + flow);
	float wisp = BackdropNoise(p * 0.12 + flow * 2.4 + 5.7);
	return smoothstep(0.52, 0.8, shape * 0.82 + wisp * 0.26);
}

vec3 DeckColor(float storm, float density)
{
	vec3 lit = mix(vec3(0.92, 0.9, 0.86), vec3(0.2, 0.22, 0.28), storm);
	vec3 shade = mix(vec3(0.5, 0.57, 0.7), vec3(0.06, 0.07, 0.1), storm);
	return mix(shade, lit, 0.45 + 0.55 * density);
}

vec3 ThroughDeck(vec3 color, vec3 origin, vec3 target, float time, float storm)
{
	if (origin.y <= StormDeck || target.y >= StormDeck)
		return color;
	vec3 crossing = mix(origin, target, (origin.y - StormDeck) / max(origin.y - target.y, 1e-3));
	float density = DeckDensity(crossing.xz, time);
	return mix(color, DeckColor(storm, density), density * 0.88);
}

vec3 StormSky(vec3 origin, vec3 direction, float time, vec3 toLight, vec4 eye, float storm)
{
	float down = max(-direction.y, 1e-3);
	vec2 sea = PlaneHit(origin, direction, StormSea);
	vec2 flow = vec2(0.004, 0.0016) * time;
	float shape = BackdropFbm(sea * 0.012 + flow);
	vec2 sunward = normalize(toLight.xz + vec2(1e-4));
	float toward = BackdropFbm((sea + sunward * 14.0) * 0.012 + flow);
	float relief = clamp(0.64 + (shape - toward) * 2.6, 0.38, 1.12);
	vec3 bright = mix(vec3(0.5, 0.58, 0.74), vec3(1.0, 0.96, 0.9), smoothstep(0.45, 0.92, shape)) * relief;
	vec3 day = mix(vec3(0.24, 0.35, 0.52), bright, smoothstep(0.3, 0.62, shape));

	float churn = BackdropFbm(sea * 0.017 + flow * 2.5 + 7.3);
	vec3 dark = mix(vec3(0.03, 0.034, 0.05), vec3(0.12, 0.13, 0.175), smoothstep(0.4, 0.92, churn)) * relief;
	float cover = clamp(storm * 1.5 - 0.35 + (BackdropFbm(sea * 0.005 + 3.1) - 0.5) * 0.9, 0.0, 1.0);
	cover = cover * cover * (3.0 - 2.0 * cover);
	vec3 color = mix(day, dark, cover);

	vec2 cell = floor(sea / 30.0);
	float beat = time * 1.3;
	float lit = BackdropHash(cell + vec2(floor(beat) * 7.31, 3.7));
	float pulse = step(0.86, lit) * (1.0 - smoothstep(0.0, 0.3, fract(beat))) * (0.6 + 0.4 * sin(time * 47.0 + lit * 30.0));
	float spot = 1.0 - smoothstep(2.5, 13.0, length(sea - (cell + 0.5) * 30.0));
	color += vec3(0.45, 0.55, 0.95) * (pulse * spot * cover * smoothstep(0.35, 0.75, churn));

	float sun = eye.w > 0.0 ? 1.0 - smoothstep(eye.z * 1.2, eye.z * 1.2 + 20.0, length(sea - eye.xy)) : 0.0;
	color += vec3(0.32, 0.24, 0.13) * (sun * cover * smoothstep(0.4, 0.85, shape));

	color = ThroughDeck(color, origin, origin + direction * ((origin.y - StormSea) / down), time, storm);
	vec3 horizon = mix(vec3(0.62, 0.7, 0.82), vec3(0.06, 0.07, 0.1), storm);
	return mix(color, horizon, pow(1.0 - down, 3.0) * 0.6);
}

vec3 Backdrop(vec3 origin, vec3 direction, float time, vec3 toLight, sampler2D island, vec4 islandTransform, float leviathan)
{
	float down = max(-direction.y, 1e-3);
	vec3 color = mix(vec3(0.024, 0.014, 0.05), vec3(0.008, 0.005, 0.02), smoothstep(0.3, 0.95, down));

	vec2 stars = PlaneHit(origin, direction, AbyssHeight) * 0.07;
	vec2 cell = floor(stars);
	float seed = BackdropHash(cell);
	vec2 jitter = vec2(BackdropHash(cell + 13.7), BackdropHash(cell + 71.3)) - 0.5;
	float star = smoothstep(0.1, 0.0, length(fract(stars) - 0.5 - jitter * 0.7)) * step(0.8, seed);
	color += star * mix(vec3(0.55, 0.65, 1.0), vec3(1.0, 0.8, 0.6), BackdropHash(cell + 5.1)) * (1.3 + sin(time * (1.5 + seed * 3.0) + seed * 40.0));

	vec2 caldera = PlaneHit(origin, direction, CalderaDepth) - CalderaBelow;
	float distance = length(caldera);
	float embers = BackdropFbm(caldera * 0.05 + vec2(0.0, time * 0.02));
	color += vec3(1.0, 0.3, 0.06) * (exp(-distance / 60.0) * (0.5 + 1.5 * embers) + exp(-distance / 18.0) * 3.0);

	vec2 low = PlaneHit(origin, direction, CloudLow);
	float lowShape = BackdropFbm(low * 0.016 + vec2(0.004, 0.0015) * time);
	float lowDensity = smoothstep(0.42, 0.78, lowShape);
	float warmth = exp(-length(low - CalderaBelow) / 150.0);
	vec3 lowColor = mix(vec3(0.05, 0.035, 0.1), vec3(0.11, 0.08, 0.19), smoothstep(0.5, 0.9, lowShape)) + vec3(0.32, 0.1, 0.03) * warmth * (1.0 - smoothstep(0.45, 0.85, lowShape));
	color = mix(color, lowColor, lowDensity * 0.9);

	if (leviathan >= 0.0)
	{
		vec4 beast = Leviathan(origin, direction, time, leviathan);
		color = mix(color, beast.rgb, beast.a);
	}

	vec2 high = PlaneHit(origin, direction, CloudHigh);
	float highShape = BackdropFbm(high * 0.028 + vec2(0.009, 0.004) * time);
	float highDensity = smoothstep(0.54, 0.84, highShape);
	vec2 shadowAt = high + toLight.xz * (-CloudHigh / max(toLight.y, 0.1));
	float lit = mix(0.3, 1.0, texture(island, shadowAt * islandTransform.xy + islandTransform.zw).r);
	vec3 highColor = mix(vec3(0.06, 0.05, 0.12), vec3(0.22, 0.21, 0.37), smoothstep(0.58, 0.95, highShape)) * lit + vec3(0.14, 0.05, 0.02) * (1.0 - smoothstep(0.54, 0.74, highShape));
	color = mix(color, highColor, highDensity * 0.8);

	return mix(color, vec3(0.05, 0.04, 0.1), pow(1.0 - down, 4.0) * 0.7);
}
