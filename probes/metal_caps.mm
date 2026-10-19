#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstdio>
int main() { @autoreleasepool {
 id<MTLDevice> d=MTLCreateSystemDefaultDevice();
 if (!d) { puts("No Metal device"); return 1; }
 printf("device=%s\nraytracing=%d\nfunctionPointers=%d\nraytracingFromRender=%d\n",d.name.UTF8String,d.supportsRaytracing,d.supportsFunctionPointers,d.supportsRaytracingFromRender);
} }
