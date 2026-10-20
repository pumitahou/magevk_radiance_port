#version 460
#extension GL_EXT_ray_tracing : require
struct Payload { uvec4 data[16]; };
layout(location=0) rayPayloadInEXT Payload payload;
hitAttributeEXT vec2 barycentrics;
void main() {
    for (uint i = 0; i < 16; ++i) {
        uint base = 123u + (gl_LaunchIDEXT.x & 7u) + uint(gl_PrimitiveID) * 17u + i * 4u;
        payload.data[i] = base + uvec4(0,1,2,3);
    }
}
