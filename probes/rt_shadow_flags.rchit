#version 460
#extension GL_EXT_ray_tracing : require
struct ShadowPayload { vec3 throughput; uint anyHitCount; vec3 radiance; uint pad; };
layout(location=0) rayPayloadInEXT ShadowPayload payload;
hitAttributeEXT vec2 barycentrics;
// Production shadow.rchit is empty. Keeping this empty proves that skipping
// closest-hit preserves shadow results while the flags terminate traversal.
void main() {}
