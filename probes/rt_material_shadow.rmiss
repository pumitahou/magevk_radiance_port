#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#include "util/ray_payloads.glsl"
layout(location=1) rayPayloadInEXT ShadowRay shadow;
void main() {
    shadow.radiance=vec3(float(gl_LaunchIDEXT.x%16u)/16.0,0.25,0.5);
    shadow.throughput=vec3(0.75);
}
