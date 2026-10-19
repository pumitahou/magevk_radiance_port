#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#include "util/ray.glsl"
layout(location=0) rayPayloadInEXT MainRay payload;
void main() { rayStoreMaterial(payload,vec4(-1),vec3(-1),-1,-1,-1,-1,-1); }
