#version 460
#extension GL_EXT_ray_tracing : enable
#include "common.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;


const vec3 sunDir = normalize(vec3(1));
vec3 SampleEnvironment(vec3 dir)
{
    const vec3 colGround = vec3(0.9f);
    const vec3 colSkyHorizon = vec3(1.0f);
    const vec3 colSkyZenith = vec3(0.08, 0.37, 0.73);

    float sun = pow(max(0, dot(dir, sunDir)), 100) * 100;
    float skyGradientT = pow(smoothstep(0, 0.4, dir.y), 0.45);
    float groundToSkyT = smoothstep(-0.01, 0.0, dir.y);
    vec3 skyGradient = mix(colSkyHorizon, colSkyZenith, skyGradientT);

    return mix(colGround, skyGradient, groundToSkyT) + sun * float(groundToSkyT >= 1.0);
}

void main()
{
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    payload.color = SampleEnvironment(dir);
    //payload.color = vec3(1, 0, 0);
    payload.origin = vec3(0);
    payload.normal = vec3(0);
    payload.hitDistance = 1e30;
    payload.didHit = 0;
}