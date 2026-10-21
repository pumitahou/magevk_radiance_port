#define VK_USE_PLATFORM_METAL_EXT
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#define CHECK(call) do { VkResult r=(call); if (r!=VK_SUCCESS) { fprintf(stderr,"%s: %d\n",#call,r); throw std::runtime_error("Vulkan failure"); } } while(0)
int main(int argc, char** argv) { @autoreleasepool { try {
    int seconds=argc>1 ? atoi(argv[1]) : 15;
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    NSWindow* window=[[NSWindow alloc] initWithContentRect:NSMakeRect(40,40,320,180)
        styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
    window.releasedWhenClosed=NO;
    CAMetalLayer* layer=[CAMetalLayer layer];
    layer.device=MTLCreateSystemDefaultDevice(); layer.pixelFormat=MTLPixelFormatBGRA8Unorm;
    layer.drawableSize=CGSizeMake(320,180); layer.framebufferOnly=NO;
    window.contentView.wantsLayer=YES; window.contentView.layer=layer;
    [window orderFrontRegardless];
    const char* extensions[]={VK_KHR_SURFACE_EXTENSION_NAME,VK_EXT_METAL_SURFACE_EXTENSION_NAME,
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion=VK_API_VERSION_1_2;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ci.pApplicationInfo=&app;
    ci.flags=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR; ci.enabledExtensionCount=3; ci.ppEnabledExtensionNames=extensions;
    VkInstance instance; CHECK(vkCreateInstance(&ci,nullptr,&instance));
    VkMetalSurfaceCreateInfoEXT sci{VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT}; sci.pLayer=layer;
    VkSurfaceKHR surface; auto createSurface=(PFN_vkCreateMetalSurfaceEXT)vkGetInstanceProcAddr(instance,"vkCreateMetalSurfaceEXT");
    CHECK(createSurface(instance,&sci,nullptr,&surface));
    uint32_t count=0; CHECK(vkEnumeratePhysicalDevices(instance,&count,nullptr));
    std::vector<VkPhysicalDevice> physical(count); CHECK(vkEnumeratePhysicalDevices(instance,&count,physical.data()));
    if (!count) throw std::runtime_error("No device"); auto gpu=physical[0];
    vkGetPhysicalDeviceQueueFamilyProperties(gpu,&count,nullptr); std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu,&count,families.data()); uint32_t family=UINT32_MAX;
    for(uint32_t i=0;i<count;i++){VkBool32 present;CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu,i,surface,&present));if(present&&(families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)){family=i;break;}}
    if(family==UINT32_MAX) throw std::runtime_error("No present queue");
    const char* devExt[]={VK_KHR_SWAPCHAIN_EXTENSION_NAME,"VK_KHR_portability_subset"};
    float priority=1; VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qi.queueFamilyIndex=family; qi.queueCount=1; qi.pQueuePriorities=&priority;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;di.enabledExtensionCount=2;di.ppEnabledExtensionNames=devExt;
    VkDevice device; CHECK(vkCreateDevice(gpu,&di,nullptr,&device)); VkQueue queue;vkGetDeviceQueue(device,family,0,&queue);
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=family;pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool;CHECK(vkCreateCommandPool(device,&pi,nullptr,&pool));
    constexpr int frames=2; VkCommandBuffer cb[frames]; VkFence fences[frames];VkSemaphore acquired[frames];
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=frames;
    CHECK(vkAllocateCommandBuffers(device,&ai,cb));
    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};fi.flags=VK_FENCE_CREATE_SIGNALED_BIT;
    for(int i=0;i<frames;i++){CHECK(vkCreateFence(device,&fi,nullptr,&fences[i]));CHECK(vkCreateSemaphore(device,&semInfo,nullptr,&acquired[i]));}
    uint64_t submitted=0;int generations=0;
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
    while(std::chrono::steady_clock::now()<end){
      VkSurfaceCapabilitiesKHR caps;CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu,surface,&caps));
      uint32_t nf=0;CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu,surface,&nf,nullptr));std::vector<VkSurfaceFormatKHR> formats(nf);CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu,surface,&nf,formats.data()));
      auto format=formats[0];for(auto f:formats)if(f.format==VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)format=f;
      VkSwapchainCreateInfoKHR si{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};si.surface=surface;si.minImageCount=std::max(3u,caps.minImageCount);if(caps.maxImageCount)si.minImageCount=std::min(si.minImageCount,caps.maxImageCount);
      si.imageFormat=format.format;si.imageColorSpace=format.colorSpace;si.imageExtent=caps.currentExtent;
      if(si.imageExtent.width==UINT32_MAX)si.imageExtent={320,180};
      si.imageArrayLayers=1;si.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT;si.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
      si.preTransform=caps.currentTransform;si.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;si.presentMode=VK_PRESENT_MODE_FIFO_KHR;si.clipped=VK_TRUE;
      VkSwapchainKHR swapchain;CHECK(vkCreateSwapchainKHR(device,&si,nullptr,&swapchain));
      uint32_t ni=0;CHECK(vkGetSwapchainImagesKHR(device,swapchain,&ni,nullptr));std::vector<VkImage> images(ni);CHECK(vkGetSwapchainImagesKHR(device,swapchain,&ni,images.data()));
      std::vector<VkSemaphore> ready(ni);std::vector<bool> initialized(ni,false);for(auto& s:ready)CHECK(vkCreateSemaphore(device,&semInfo,nullptr,&s));
      auto until=std::min(end,std::chrono::steady_clock::now()+std::chrono::seconds(2));
      while(std::chrono::steady_clock::now()<until){ @autoreleasepool {
        while(NSEvent* event=[NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES])[NSApp sendEvent:event];
        int f=submitted%frames;CHECK(vkWaitForFences(device,1,&fences[f],VK_TRUE,10000000000ull));
        uint32_t image;CHECK(vkAcquireNextImageKHR(device,swapchain,10000000000ull,acquired[f],VK_NULL_HANDLE,&image));
        CHECK(vkResetFences(device,1,&fences[f]));CHECK(vkResetCommandBuffer(cb[f],0));
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;CHECK(vkBeginCommandBuffer(cb[f],&bi));
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.oldLayout=initialized[image]?VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=images[image];b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(cb[f],VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
        VkClearColorValue color{{0.05f,0.1f,0.15f,1.0f}};vkCmdClearColorImage(cb[f],images[image],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&b.subresourceRange);
        b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=0;
        vkCmdPipelineBarrier(cb[f],VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,nullptr,0,nullptr,1,&b);CHECK(vkEndCommandBuffer(cb[f]));
        VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TRANSFER_BIT;VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&acquired[f];submit.pWaitDstStageMask=&stage;submit.commandBufferCount=1;submit.pCommandBuffers=&cb[f];submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&ready[image];CHECK(vkQueueSubmit(queue,1,&submit,fences[f]));
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};present.waitSemaphoreCount=1;present.pWaitSemaphores=&ready[image];present.swapchainCount=1;present.pSwapchains=&swapchain;present.pImageIndices=&image;CHECK(vkQueuePresentKHR(queue,&present));
        initialized[image]=true;submitted++;
      }}
      CHECK(vkDeviceWaitIdle(device));vkDestroySwapchainKHR(device,swapchain,nullptr);for(auto s:ready)vkDestroySemaphore(device,s,nullptr);generations++;
    }
    CHECK(vkDeviceWaitIdle(device));for(int f=0;f<frames;f++){vkDestroyFence(device,fences[f],nullptr);vkDestroySemaphore(device,acquired[f],nullptr);}
    vkDestroyCommandPool(device,pool,nullptr);vkDestroyDevice(device,nullptr);vkDestroySurfaceKHR(instance,surface,nullptr);vkDestroyInstance(instance,nullptr);
    [window close];[window release];
    printf("PRESENT_RESULT frames=%llu swapchains=%d seconds=%d\n",(unsigned long long)submitted,generations,seconds);
    return 0;
} catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;} }}
