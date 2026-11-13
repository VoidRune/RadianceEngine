#version 450

layout(location = 0) in vec2 inLocal;
layout(location = 1) in vec3 inColor;
layout(location = 2) flat in vec2 inShape;
layout(location = 0) out vec4 outColor;

void main()
{
	float span = inShape.y;
	vec2 q = vec2(max(abs(inLocal.x) - span, 0.0), inLocal.y);
	float r2 = dot(q, q);
	vec3 color;
	if (inShape.x < 0.5)
		color = inColor * exp(-r2 * 2.2);
	else if (inShape.x < 1.5)
		color = inColor * exp(-r2 * 1.4);
	else
	{
		float r = sqrt(r2);
		float disc = smoothstep(1.0, 0.82, r);
		color = inColor * disc * (0.7 + 0.3 * smoothstep(0.55, 0.95, r));
	}
	outColor = vec4(color, 0.0);
}
