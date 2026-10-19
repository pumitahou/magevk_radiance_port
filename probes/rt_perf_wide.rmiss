#version 460
#extension GL_EXT_ray_tracing : require
struct Payload { uvec4 data[16]; };
layout(location=0) rayPayloadInEXT Payload payload;
void main() {
    for (uint i = 0; i < 16; ++i) {
        uint base = 456u + (gl_LaunchIDEXT.x & 7u) + i * 4u;
        payload.data[i] = base + uvec4(0,1,2,3);
    }
}
