#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#define CHECK(x) do { VkResult r=(x); if(r!=VK_SUCCESS) { fprintf(stderr,"%s: VkResult %d\n",#x,r); throw std::runtime_error(#x); } } while(0)
VkDevice dev; VkPhysicalDevice gpu; VkQueue queue; VkCommandPool pool;
PFN_vkCreateAccelerationStructureKHR createAS;
PFN_vkGetAccelerationStructureBuildSizesKHR asSizes;
PFN_vkCmdBuildAccelerationStructuresKHR buildAS;
PFN_vkGetAccelerationStructureDeviceAddressKHR asAddress;
PFN_vkCreateRayTracingPipelinesKHR createRT;
PFN_vkGetRayTracingShaderGroupHandlesKHR groupHandles;
PFN_vkCmdTraceRaysKHR traceRays;
struct Buffer { VkBuffer buffer; VkDeviceMemory memory; VkDeviceAddress address; void* mapped; };
std::vector<Buffer> buffers;
Buffer buffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data=nullptr) {
 Buffer b{}; VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; bi.size=size; bi.usage=usage|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
 CHECK(vkCreateBuffer(dev,&bi,nullptr,&b.buffer)); VkMemoryRequirements req; vkGetBufferMemoryRequirements(dev,b.buffer,&req);
 VkPhysicalDeviceMemoryProperties props; vkGetPhysicalDeviceMemoryProperties(gpu,&props);
 uint32_t index=UINT32_MAX;
 for(uint32_t i=0;i<props.memoryTypeCount;i++) if((req.memoryTypeBits&(1u<<i)) && (props.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){ index=i; break; }
 if(index==UINT32_MAX) throw std::runtime_error("No host coherent memory");
 VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO}; flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
 VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ai.pNext=&flags; ai.allocationSize=req.size; ai.memoryTypeIndex=index;
 CHECK(vkAllocateMemory(dev,&ai,nullptr,&b.memory)); CHECK(vkBindBufferMemory(dev,b.buffer,b.memory,0)); CHECK(vkMapMemory(dev,b.memory,0,VK_WHOLE_SIZE,0,&b.mapped));
 memset(b.mapped,0,size); if(data) memcpy(b.mapped,data,size);
 VkBufferDeviceAddressInfo address{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO}; address.buffer=b.buffer; b.address=vkGetBufferDeviceAddress(dev,&address);
 buffers.push_back(b); return b;
}
VkCommandBuffer begin() {
 VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; ai.commandPool=pool; ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount=1;
 VkCommandBuffer cmd; CHECK(vkAllocateCommandBuffers(dev,&ai,&cmd)); VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; CHECK(vkBeginCommandBuffer(cmd,&bi)); return cmd;
}
void submit(VkCommandBuffer cmd) {
 CHECK(vkEndCommandBuffer(cmd)); VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1; si.pCommandBuffers=&cmd;
 CHECK(vkQueueSubmit(queue,1,&si,VK_NULL_HANDLE)); CHECK(vkQueueWaitIdle(queue)); vkFreeCommandBuffers(dev,pool,1,&cmd);
}
VkAccelerationStructureKHR acceleration(VkAccelerationStructureTypeKHR type, VkAccelerationStructureGeometryKHR geom) {
 VkAccelerationStructureBuildGeometryInfoKHR info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR}; info.type=type; info.flags=VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR; info.mode=VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR; info.geometryCount=1; info.pGeometries=&geom;
 uint32_t count=1; VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR}; asSizes(dev,VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,&info,&count,&sizes);
 auto storage=buffer(sizes.accelerationStructureSize,VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
 VkAccelerationStructureCreateInfoKHR ci{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR}; ci.type=type; ci.buffer=storage.buffer; ci.size=sizes.accelerationStructureSize;
 VkAccelerationStructureKHR as; CHECK(createAS(dev,&ci,nullptr,&as)); info.dstAccelerationStructure=as;
 VkPhysicalDeviceAccelerationStructurePropertiesKHR ap{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR}; VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; p.pNext=&ap; vkGetPhysicalDeviceProperties2(gpu,&p);
 auto scratch=buffer(sizes.buildScratchSize+ap.minAccelerationStructureScratchOffsetAlignment,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
 auto align=VkDeviceAddress(ap.minAccelerationStructureScratchOffsetAlignment); info.scratchData.deviceAddress=(scratch.address+align-1)&~(align-1);
 VkAccelerationStructureBuildRangeInfoKHR range{}; range.primitiveCount=1; auto rp=&range;
 auto cmd=begin(); buildAS(cmd,1,&info,&rp);
 VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask=VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR; barrier.dstAccessMask=VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
 vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,1,&barrier,0,nullptr,0,nullptr); submit(cmd);
 return as;
}
VkShaderModule shader(const char* path) {
 std::ifstream f(path,std::ios::binary|std::ios::ate); if(!f) throw std::runtime_error(path); auto size=size_t(f.tellg()); std::vector<uint32_t> words((size+3)/4); f.seekg(0); f.read((char*)words.data(),size);
 VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; ci.codeSize=size; ci.pCode=words.data(); VkShaderModule s; CHECK(vkCreateShaderModule(dev,&ci,nullptr,&s)); return s;
}

struct Image {VkImage image; VkDeviceMemory memory; VkImageView view; uint32_t width,height;};
Image image(uint32_t width,uint32_t height,VkFormat format) {
 Image i{};i.width=width;i.height=height;VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;ci.extent={width,height,1};ci.mipLevels=ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;CHECK(vkCreateImage(dev,&ci,nullptr,&i.image));
 VkMemoryRequirements req;vkGetImageMemoryRequirements(dev,i.image,&req);uint32_t index=0;while(!(req.memoryTypeBits&(1u<<index)))index++;VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;ai.memoryTypeIndex=index;CHECK(vkAllocateMemory(dev,&ai,nullptr,&i.memory));CHECK(vkBindImageMemory(dev,i.image,i.memory,0));
 VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=i.image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};CHECK(vkCreateImageView(dev,&vi,nullptr,&i.view));return i;
}
void barrier(VkCommandBuffer cmd,Image i,VkImageLayout before,VkImageLayout after,VkPipelineStageFlags from,VkPipelineStageFlags to,VkAccessFlags src,VkAccessFlags dst) {
 VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=src;b.dstAccessMask=dst;b.oldLayout=before;b.newLayout=after;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=i.image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdPipelineBarrier(cmd,from,to,0,0,nullptr,0,nullptr,1,&b);
}
int main(int argc,char**argv) {setvbuf(stdout,nullptr,_IONBF,0);try {
 uint32_t iw=argc>1?std::stoul(argv[1]):120,ih=argc>2?std::stoul(argv[2]):68,ow=argc>3?std::stoul(argv[3]):480,oh=argc>4?std::stoul(argv[4]):272;
 std::string pattern=argc>5?argv[5]:"flat";float sharpness=argc>6?std::stof(argv[6]):0.12f;std::string shaders=argc>7?argv[7]:"experiments/E15-fsr1";
 if(!iw||!ih||!ow||!oh||uint64_t(iw)*ih>1048576||uint64_t(ow)*oh>4194304||!std::isfinite(sharpness)||sharpness<0||sharpness>1)throw std::runtime_error("Probe bounds exceeded");
 VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="Radiance E15 FSR1 compute probe";app.apiVersion=VK_API_VERSION_1_3;const char*ie[]={"VK_KHR_portability_enumeration"};VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.flags=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;ici.pApplicationInfo=&app;ici.enabledExtensionCount=1;ici.ppEnabledExtensionNames=ie;VkInstance instance;CHECK(vkCreateInstance(&ici,nullptr,&instance));uint32_t count=1;CHECK(vkEnumeratePhysicalDevices(instance,&count,&gpu));VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(gpu,&properties);
 uint32_t qc=0;vkGetPhysicalDeviceQueueFamilyProperties(gpu,&qc,nullptr);std::vector<VkQueueFamilyProperties> qps(qc);vkGetPhysicalDeviceQueueFamilyProperties(gpu,&qc,qps.data());uint32_t qi=0;while(qi<qc&&!(qps[qi].queueFlags&VK_QUEUE_COMPUTE_BIT))qi++;if(qi==qc)throw std::runtime_error("No compute queue");
 float priority=1;VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qci.queueFamilyIndex=qi;qci.queueCount=1;qci.pQueuePriorities=&priority;VkPhysicalDeviceBufferDeviceAddressFeatures bda{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};bda.bufferDeviceAddress=VK_TRUE;const char*de[]={"VK_KHR_portability_subset"};VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.pNext=&bda;dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;dci.enabledExtensionCount=1;dci.ppEnabledExtensionNames=de;CHECK(vkCreateDevice(gpu,&dci,nullptr,&dev));vkGetDeviceQueue(dev,qi,0,&queue);VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pci.queueFamilyIndex=qi;CHECK(vkCreateCommandPool(dev,&pci,nullptr,&pool));
 auto source=image(iw,ih,VK_FORMAT_R16G16B16A16_SFLOAT),middle=image(ow,oh,VK_FORMAT_R32G32B32A32_SFLOAT),target=image(ow,oh,VK_FORMAT_R16G16B16A16_SFLOAT);
 std::vector<_Float16> pixels(size_t(iw)*ih*4);
 auto value=[&](uint32_t x,uint32_t y,int c)->float {
  if(c==3)return 1;
  if(pattern=="flat"){float v[]={0.18f,0.5f,1};return v[c];}
  if(pattern=="hdr"){float v[]={0,4,32};return v[c];}
  if(pattern=="gradient")return c==0?float(x)/std::max(1u,iw-1):c==1?float(y)/std::max(1u,ih-1):0.25f;
  if(pattern=="edge"){bool side=uint64_t(x)*ih>uint64_t(y)*iw;return c==1?0.5f:(c==0)==side?4.0f:0.125f;}
  throw std::runtime_error("Unknown pattern");};
 for(uint32_t y=0;y<ih;y++)for(uint32_t x=0;x<iw;x++)for(int c=0;c<4;c++)pixels[(size_t(y)*iw+x)*4+c]=value(x,y,c);
 auto upload=buffer(pixels.size()*sizeof(_Float16),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,pixels.data());auto readback=buffer(VkDeviceSize(ow)*oh*8,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
 auto initial=begin();barrier(initial,source,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT);VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={iw,ih,1};vkCmdCopyBufferToImage(initial,upload.buffer,source.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);barrier(initial,source,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);barrier(initial,middle,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,VK_ACCESS_SHADER_WRITE_BIT);barrier(initial,target,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,VK_ACCESS_SHADER_WRITE_BIT);submit(initial);
 VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};samplerInfo.magFilter=samplerInfo.minFilter=VK_FILTER_LINEAR;samplerInfo.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;samplerInfo.addressModeU=samplerInfo.addressModeV=samplerInfo.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;VkSampler sampler;CHECK(vkCreateSampler(dev,&samplerInfo,nullptr,&sampler));
 VkDescriptorSetLayoutBinding bindings[]={{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{2,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};lci.bindingCount=3;lci.pBindings=bindings;VkDescriptorSetLayout layout;CHECK(vkCreateDescriptorSetLayout(dev,&lci,nullptr,&layout));VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,4};VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};plci.setLayoutCount=1;plci.pSetLayouts=&layout;plci.pushConstantRangeCount=1;plci.pPushConstantRanges=&push;VkPipelineLayout pipelineLayout;CHECK(vkCreatePipelineLayout(dev,&plci,nullptr,&pipelineLayout));
 VkPipeline pipelines[2];const char*names[]={"fsr1_easu_comp.spv","fsr1_rcas_comp.spv"};for(int i=0;i<2;i++){VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};ci.layout=pipelineLayout;ci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;ci.stage.module=shader((shaders+"/"+names[i]).c_str());ci.stage.pName="main";CHECK(vkCreateComputePipelines(dev,VK_NULL_HANDLE,1,&ci,nullptr,&pipelines[i]));}
 VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,2}};VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dpci.maxSets=1;dpci.poolSizeCount=2;dpci.pPoolSizes=sizes;VkDescriptorPool dp;CHECK(vkCreateDescriptorPool(dev,&dpci,nullptr,&dp));VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};dsai.descriptorPool=dp;dsai.descriptorSetCount=1;dsai.pSetLayouts=&layout;VkDescriptorSet ds;CHECK(vkAllocateDescriptorSets(dev,&dsai,&ds));VkDescriptorImageInfo infos[]={{sampler,source.view,VK_IMAGE_LAYOUT_GENERAL},{VK_NULL_HANDLE,middle.view,VK_IMAGE_LAYOUT_GENERAL},{VK_NULL_HANDLE,target.view,VK_IMAGE_LAYOUT_GENERAL}};VkWriteDescriptorSet writes[3]{};for(uint32_t i=0;i<3;i++){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=ds;writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=i?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[i].pImageInfo=&infos[i];}vkUpdateDescriptorSets(dev,3,writes,0,nullptr);
 VkQueryPoolCreateInfo qpi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};qpi.queryType=VK_QUERY_TYPE_TIMESTAMP;qpi.queryCount=2;VkQueryPool timestamps;CHECK(vkCreateQueryPool(dev,&qpi,nullptr,&timestamps));std::vector<double> times;double maxError=0,meanError=0;
 for(int repetition=0;repetition<6;repetition++){
  auto cmd=begin();if(repetition)barrier(cmd,target,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT);barrier(cmd,middle,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT);vkCmdResetQueryPool(cmd,timestamps,0,2);vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipelineLayout,0,1,&ds,0,nullptr);vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,timestamps,0);vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipelines[0]);vkCmdDispatch(cmd,(ow+15)/16,(oh+15)/16,1);barrier(cmd,middle,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipelines[1]);vkCmdPushConstants(cmd,pipelineLayout,VK_SHADER_STAGE_COMPUTE_BIT,0,4,&sharpness);vkCmdDispatch(cmd,(ow+15)/16,(oh+15)/16,1);vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timestamps,1);barrier(cmd,target,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);copy.imageExtent={ow,oh,1};vkCmdCopyImageToBuffer(cmd,target.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};mb.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;mb.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&mb,0,nullptr,0,nullptr);submit(cmd);
  uint64_t ticks[2];CHECK(vkGetQueryPoolResults(dev,timestamps,0,2,sizeof(ticks),ticks,8,VK_QUERY_RESULT_64_BIT));if(repetition)times.push_back(double(ticks[1]-ticks[0])*properties.limits.timestampPeriod/1e6);
  auto values=static_cast<_Float16*>(readback.mapped);maxError=meanError=0;
  for(uint32_t y=0;y<oh;y++)for(uint32_t x=0;x<ow;x++)for(int c=0;c<4;c++){
   float actual=values[(size_t(y)*ow+x)*4+c];if(!std::isfinite(actual)||actual<0||actual>65504)throw std::runtime_error("Nonfinite or out-of-range output");if(c==3&&actual!=1)throw std::runtime_error("Incorrect output alpha");
   float expected=actual;
   if(pattern=="flat"||pattern=="hdr")expected=value(0,0,c);
   if(pattern=="gradient"&&c<3){float sx=std::clamp((float(x)+0.5f)*iw/ow-0.5f,0.0f,float(iw-1)),sy=std::clamp((float(y)+0.5f)*ih/oh-0.5f,0.0f,float(ih-1));expected=c==0?sx/std::max(1u,iw-1):c==1?sy/std::max(1u,ih-1):0.25f;}
   if(pattern=="edge"&&((uint64_t(x)*oh>uint64_t(y)*ow+uint64_t(ow)*32)||(uint64_t(y)*ow>uint64_t(x)*oh+uint64_t(ow)*32)))expected=value(std::min(iw-1,uint32_t(uint64_t(x)*iw/ow)),std::min(ih-1,uint32_t(uint64_t(y)*ih/oh)),c);
   double error=std::abs(actual-expected)/std::max(1.0f,expected);maxError=std::max(maxError,error);meanError+=error;
  }
  meanError/=double(ow)*oh*4;if((pattern=="gradient"&&meanError>0.025)||((pattern=="flat"||pattern=="hdr"||pattern=="edge")&&maxError>0.025)){fprintf(stderr,"Pattern %s: max_error=%.6f mean_error=%.6f output0=%.6f,%.6f,%.6f\n",pattern.c_str(),maxError,meanError,float(values[0]),float(values[1]),float(values[2]));throw std::runtime_error("Analytic pattern comparison failed");}
 }
 std::sort(times.begin(),times.end());printf("RESULT {\"input_width\":%u,\"input_height\":%u,\"output_width\":%u,\"output_height\":%u,\"pattern\":\"%s\",\"sharpness\":%.3f,\"gpu_ms_median\":%.6f,\"max_relative_error\":%.7f,\"mean_relative_error\":%.7f,\"all_pixels_checked\":true}\n",iw,ih,ow,oh,pattern.c_str(),sharpness,times[times.size()/2],maxError,meanError);
 return 0;
 }catch(const std::exception&e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
