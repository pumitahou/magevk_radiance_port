#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#include "util/ray.glsl"
#include "util/alpha_mode.glsl"
#include "util/labpbr.glsl"
layout(location=0) rayPayloadInEXT MainRay payload;
layout(location=1) rayPayloadEXT ShadowRay shadow;
layout(set=0,binding=0) uniform accelerationStructureEXT scene;
hitAttributeEXT vec2 barycentric;
void main() {
    float alpha=resolveSurfaceAlpha((payload.seed&1u)==0u?0.25:0.75,ALPHA_MODE_TRANSPARENT);
    vec4 albedo=vec4(float(gl_LaunchIDEXT.x%16u)/16.0,float(gl_LaunchIDEXT.y%16u)/16.0,0.5,alpha);
#if MCVR_TEST_GLASS_SPECULAR
    vec4 specular=vec4(1.0,10.0/255.0,0.0,1.0);
#else
    vec4 specular=LABPBR_DEFAULT_SPECULAR;
#endif
    LabPBRMat m=convertLabPBRMaterial(albedo,specular,LABPBR_DEFAULT_NORMAL);
    rayStoreMaterial(payload,albedo,m.f0,m.roughness,m.metallic,m.transmission,m.ior,m.emission);
    shadow.radiance=vec3(0); shadow.throughput=vec3(0); shadow.insideBoat=0; shadow.pad0=0;
    // A different payload and a miss shader exercise the nested shadow-call ABI.
    traceRayEXT(scene,gl_RayFlagsNoneEXT,0,0,0,1,vec3(0,0,-1),0.001,vec3(0,0,1),10,1);
    if (abs(shadow.radiance.x-float(gl_LaunchIDEXT.x%16u)/16.0)>0.001 ||
        abs(shadow.throughput.x-0.75)>0.001) {
        rayStoreMaterial(payload,vec4(-1),vec3(-1),-1,-1,-1,-1,-1);
    }
}
