#version 460
#extension GL_EXT_ray_tracing : require
struct ShadowPayload { vec3 throughput; uint anyHitCount; vec3 radiance; uint pad; };
layout(location=0) rayPayloadInEXT ShadowPayload payload;
void main() { payload.radiance = payload.throughput; }
