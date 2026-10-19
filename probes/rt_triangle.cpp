#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <stdexcept>
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
int main() { try {
 VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.pApplicationName="Radiance M4 RT triangle probe"; app.apiVersion=VK_API_VERSION_1_2;
 const char* ie[]={"VK_KHR_portability_enumeration"}; VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo=&app; ici.flags=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR; ici.enabledExtensionCount=1; ici.ppEnabledExtensionNames=ie; VkInstance instance; CHECK(vkCreateInstance(&ici,nullptr,&instance));
 uint32_t n=1; CHECK(vkEnumeratePhysicalDevices(instance,&n,&gpu)); VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(gpu,&props); printf("GPU: %s\n",props.deviceName);
 uint32_t qn=0; vkGetPhysicalDeviceQueueFamilyProperties(gpu,&qn,nullptr); std::vector<VkQueueFamilyProperties> qp(qn); vkGetPhysicalDeviceQueueFamilyProperties(gpu,&qn,qp.data()); uint32_t qi=0; while(qi<qn && !(qp[qi].queueFlags&VK_QUEUE_COMPUTE_BIT)) qi++; if(qi==qn) throw std::runtime_error("No compute queue");
 float priority=1; VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qci.queueFamilyIndex=qi; qci.queueCount=1; qci.pQueuePriorities=&priority;
 VkPhysicalDeviceBufferDeviceAddressFeatures bda{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES}; bda.bufferDeviceAddress=VK_TRUE;
 VkPhysicalDeviceAccelerationStructureFeaturesKHR af{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR}; af.accelerationStructure=VK_TRUE; af.pNext=&bda;
 VkPhysicalDeviceRayTracingPipelineFeaturesKHR rf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR}; rf.rayTracingPipeline=VK_TRUE; rf.pNext=&af;
 const char* de[]={"VK_KHR_portability_subset","VK_KHR_acceleration_structure","VK_KHR_ray_tracing_pipeline","VK_KHR_deferred_host_operations"};
 VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dci.pNext=&rf; dci.queueCreateInfoCount=1; dci.pQueueCreateInfos=&qci; dci.enabledExtensionCount=4; dci.ppEnabledExtensionNames=de; CHECK(vkCreateDevice(gpu,&dci,nullptr,&dev)); vkGetDeviceQueue(dev,qi,0,&queue);
#define LOAD(v, name) v=(PFN_##name)vkGetDeviceProcAddr(dev,#name); if(!v) throw std::runtime_error(#name)
 LOAD(createAS,vkCreateAccelerationStructureKHR); LOAD(asSizes,vkGetAccelerationStructureBuildSizesKHR); LOAD(buildAS,vkCmdBuildAccelerationStructuresKHR); LOAD(asAddress,vkGetAccelerationStructureDeviceAddressKHR); LOAD(createRT,vkCreateRayTracingPipelinesKHR); LOAD(groupHandles,vkGetRayTracingShaderGroupHandlesKHR); LOAD(traceRays,vkCmdTraceRaysKHR);
 VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pci.queueFamilyIndex=qi; CHECK(vkCreateCommandPool(dev,&pci,nullptr,&pool));
 const float vertices[]={-1,-1,0,1,-1,0,0,1,0}; auto vb=buffer(sizeof(vertices),VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,vertices);
 VkAccelerationStructureGeometryKHR geom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR}; geom.geometryType=VK_GEOMETRY_TYPE_TRIANGLES_KHR; geom.flags=VK_GEOMETRY_OPAQUE_BIT_KHR;
 auto& tri=geom.geometry.triangles; tri.sType=VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR; tri.vertexFormat=VK_FORMAT_R32G32B32_SFLOAT; tri.vertexData.deviceAddress=vb.address; tri.vertexStride=12; tri.maxVertex=2; tri.indexType=VK_INDEX_TYPE_NONE_KHR;
 auto blas=acceleration(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,geom); puts("BLAS built");
 VkAccelerationStructureDeviceAddressInfoKHR adi{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR}; adi.accelerationStructure=blas;
 VkAccelerationStructureInstanceKHR ins{}; ins.transform.matrix[0][0]=ins.transform.matrix[1][1]=ins.transform.matrix[2][2]=1; ins.mask=255; ins.flags=VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR; ins.accelerationStructureReference=asAddress(dev,&adi);
 auto ib=buffer(sizeof(ins),VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,&ins); VkAccelerationStructureGeometryKHR ig{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR}; ig.geometryType=VK_GEOMETRY_TYPE_INSTANCES_KHR; ig.geometry.instances.sType=VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR; ig.geometry.instances.data.deviceAddress=ib.address;
 auto tlas=acceleration(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,ig); puts("TLAS built");
 VkDescriptorSetLayoutBinding bindings[]={{0,VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,1,VK_SHADER_STAGE_RAYGEN_BIT_KHR,nullptr},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_RAYGEN_BIT_KHR,nullptr}};
 VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; lci.bindingCount=2; lci.pBindings=bindings; VkDescriptorSetLayout layout; CHECK(vkCreateDescriptorSetLayout(dev,&lci,nullptr,&layout));
 VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; plci.setLayoutCount=1; plci.pSetLayouts=&layout; VkPipelineLayout pl; CHECK(vkCreatePipelineLayout(dev,&plci,nullptr,&pl));
 VkShaderModule shaders[]={shader("probes/triangle.rgen.spv"),shader("probes/triangle.rmiss.spv"),shader("probes/triangle.rchit.spv")}; VkShaderStageFlagBits stages[]={VK_SHADER_STAGE_RAYGEN_BIT_KHR,VK_SHADER_STAGE_MISS_BIT_KHR,VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR};
 VkPipelineShaderStageCreateInfo sci[3]{}; VkRayTracingShaderGroupCreateInfoKHR groups[3]{};
 for(int i=0;i<3;i++){sci[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;sci[i].stage=stages[i];sci[i].module=shaders[i];sci[i].pName="main";groups[i].sType=VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;groups[i].type=i==2?VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR:VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;groups[i].generalShader=i==2?VK_SHADER_UNUSED_KHR:i;groups[i].closestHitShader=i==2?2:VK_SHADER_UNUSED_KHR;groups[i].anyHitShader=groups[i].intersectionShader=VK_SHADER_UNUSED_KHR;}
 VkRayTracingPipelineCreateInfoKHR rci{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR}; rci.stageCount=3;rci.pStages=sci;rci.groupCount=3;rci.pGroups=groups;rci.maxPipelineRayRecursionDepth=1;rci.layout=pl; VkPipeline pipeline; CHECK(createRT(dev,VK_NULL_HANDLE,VK_NULL_HANDLE,1,&rci,nullptr,&pipeline)); puts("RT pipeline created");
 VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR}; VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};p2.pNext=&rt;vkGetPhysicalDeviceProperties2(gpu,&p2);
 VkDeviceSize stride=(rt.shaderGroupHandleSize+rt.shaderGroupBaseAlignment-1)&~(VkDeviceSize(rt.shaderGroupBaseAlignment)-1); auto sbt=buffer(stride*4,VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR); auto base=(sbt.address+rt.shaderGroupBaseAlignment-1)&~(VkDeviceAddress(rt.shaderGroupBaseAlignment)-1); auto offset=base-sbt.address;
 std::vector<char> handles(rt.shaderGroupHandleSize*3); CHECK(groupHandles(dev,pipeline,0,3,handles.size(),handles.data()));for(int i=0;i<3;i++) memcpy((char*)sbt.mapped+offset+i*stride,handles.data()+i*rt.shaderGroupHandleSize,rt.shaderGroupHandleSize);
 auto output=buffer(8,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);VkDescriptorPoolSize ps[]={{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}}; VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dpci.maxSets=1;dpci.poolSizeCount=2;dpci.pPoolSizes=ps;VkDescriptorPool dp;CHECK(vkCreateDescriptorPool(dev,&dpci,nullptr,&dp)); VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};dsai.descriptorPool=dp;dsai.descriptorSetCount=1;dsai.pSetLayouts=&layout;VkDescriptorSet ds;CHECK(vkAllocateDescriptorSets(dev,&dsai,&ds));
 VkWriteDescriptorSetAccelerationStructureKHR aw{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};aw.accelerationStructureCount=1;aw.pAccelerationStructures=&tlas;VkDescriptorBufferInfo obi{output.buffer,0,8};VkWriteDescriptorSet writes[2]{};for(int i=0;i<2;i++){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=ds;writes[i].dstBinding=i;writes[i].descriptorCount=1;}writes[0].pNext=&aw;writes[0].descriptorType=VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;writes[1].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[1].pBufferInfo=&obi;vkUpdateDescriptorSets(dev,2,writes,0,nullptr);
 auto cmd=begin();vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,pipeline);vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,pl,0,1,&ds,0,nullptr);VkStridedDeviceAddressRegionKHR rg{base,stride,stride},miss{base+stride,stride,stride},hit{base+stride*2,stride,stride},call{};traceRays(cmd,&rg,&miss,&hit,&call,2,1,1);
 VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};mb.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;mb.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,VK_PIPELINE_STAGE_HOST_BIT,0,1,&mb,0,nullptr,0,nullptr);submit(cmd);
 auto values=(uint32_t*)output.mapped;printf("GPU results: hit=%u (expected 123), miss=%u (expected 456)\n",values[0],values[1]); bool pass=values[0]==123&&values[1]==456; puts(pass?"PASS: BLAS + TLAS + RT pipeline + GPU traversal/readback":"FAIL: unexpected GPU readback");
 // A standalone probe exits here; the OS reclaims all GPU resources.
 return pass?0:2;
 }catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;} }
