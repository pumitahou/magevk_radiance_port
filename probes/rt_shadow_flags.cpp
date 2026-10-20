#include <vulkan/vulkan.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(x) do { VkResult r = (x); if (r != VK_SUCCESS) { \
    std::fprintf(stderr, "%s: VkResult %d\n", #x, r); throw std::runtime_error(#x); } } while (0)

VkDevice dev; VkPhysicalDevice gpu; VkQueue queue; VkCommandPool pool;
PFN_vkCreateAccelerationStructureKHR createAS;
PFN_vkGetAccelerationStructureBuildSizesKHR asSizes;
PFN_vkCmdBuildAccelerationStructuresKHR buildAS;
PFN_vkGetAccelerationStructureDeviceAddressKHR asAddress;
PFN_vkCreateRayTracingPipelinesKHR createRT;
PFN_vkGetRayTracingShaderGroupHandlesKHR groupHandles;
PFN_vkCmdTraceRaysKHR traceRays;

struct Buffer { VkBuffer buffer{}; VkDeviceMemory memory{}; VkDeviceAddress address{}; void* mapped{}; };
std::vector<Buffer> buffers;

Buffer buffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data = nullptr) {
    Buffer b{};
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; bi.size = size;
    bi.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    CHECK(vkCreateBuffer(dev, &bi, nullptr, &b.buffer));
    VkMemoryRequirements req{}; vkGetBufferMemoryRequirements(dev, b.buffer, &req);
    VkPhysicalDeviceMemoryProperties props{}; vkGetPhysicalDeviceMemoryProperties(gpu, &props);
    uint32_t index = UINT32_MAX;
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        auto flags = props.memoryTypes[i].propertyFlags;
        if ((req.memoryTypeBits & (1u << i)) &&
            (flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { index = i; break; }
    }
    if (index == UINT32_MAX) throw std::runtime_error("No host coherent memory");
    VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
    flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ai.pNext = &flags;
    ai.allocationSize = req.size; ai.memoryTypeIndex = index;
    CHECK(vkAllocateMemory(dev, &ai, nullptr, &b.memory));
    CHECK(vkBindBufferMemory(dev, b.buffer, b.memory, 0));
    CHECK(vkMapMemory(dev, b.memory, 0, VK_WHOLE_SIZE, 0, &b.mapped));
    std::memset(b.mapped, 0, size); if (data) std::memcpy(b.mapped, data, size);
    VkBufferDeviceAddressInfo address{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO}; address.buffer = b.buffer;
    b.address = vkGetBufferDeviceAddress(dev, &address); buffers.push_back(b); return b;
}

VkCommandBuffer begin() {
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
    VkCommandBuffer cmd{}; CHECK(vkAllocateCommandBuffers(dev, &ai, &cmd));
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; CHECK(vkBeginCommandBuffer(cmd, &bi));
    return cmd;
}

void submit(VkCommandBuffer cmd) {
    CHECK(vkEndCommandBuffer(cmd)); VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE)); CHECK(vkQueueWaitIdle(queue));
    vkFreeCommandBuffers(dev, pool, 1, &cmd);
}

VkAccelerationStructureKHR acceleration(VkAccelerationStructureTypeKHR type,
                                         const VkAccelerationStructureGeometryKHR& geom,
                                         uint32_t primitiveCount = 1) {
    VkAccelerationStructureBuildGeometryInfoKHR info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    info.type = type; info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR; info.geometryCount = 1; info.pGeometries = &geom;
    uint32_t count = primitiveCount; VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    asSizes(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, &count, &sizes);
    auto storage = buffer(sizes.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
    VkAccelerationStructureCreateInfoKHR ci{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    ci.type = type; ci.buffer = storage.buffer; ci.size = sizes.accelerationStructureSize;
    VkAccelerationStructureKHR as{}; CHECK(createAS(dev, &ci, nullptr, &as)); info.dstAccelerationStructure = as;
    VkPhysicalDeviceAccelerationStructurePropertiesKHR ap{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 p{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; p.pNext = &ap;
    vkGetPhysicalDeviceProperties2(gpu, &p);
    auto scratch = buffer(sizes.buildScratchSize + ap.minAccelerationStructureScratchOffsetAlignment,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto align = VkDeviceAddress(ap.minAccelerationStructureScratchOffsetAlignment);
    info.scratchData.deviceAddress = (scratch.address + align - 1) & ~(align - 1);
    VkAccelerationStructureBuildRangeInfoKHR range{}; range.primitiveCount = primitiveCount; auto rp = &range;
    auto cmd = begin(); buildAS(cmd, 1, &info, &rp);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    submit(cmd); return as;
}

VkShaderModule shader(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate); if (!f) throw std::runtime_error(path);
    auto size = size_t(f.tellg()); std::vector<uint32_t> words((size + 3) / 4);
    f.seekg(0); f.read(reinterpret_cast<char*>(words.data()), size);
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; ci.codeSize = size; ci.pCode = words.data();
    VkShaderModule module{}; CHECK(vkCreateShaderModule(dev, &ci, nullptr, &module)); return module;
}

int main(int argc, char** argv) {
    try {
        uint32_t width = argc > 1 ? std::stoul(argv[1]) : 256;
        uint32_t rayCount = argc > 2 ? std::stoul(argv[2]) : 4;
        uint32_t repeats = argc > 3 ? std::stoul(argv[3]) : 7;
        if (!width || width > 1048576 || !rayCount || rayCount > 64 || !repeats || repeats > 30)
            throw std::runtime_error("Probe limits exceeded");
        constexpr uint32_t height = 5, slots = 4;
        const VkDeviceSize resultBytes = VkDeviceSize(width) * height * slots * sizeof(float) * 4;
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.pApplicationName = "Radiance RT shadow flags probe";
        app.apiVersion = VK_API_VERSION_1_2;
        const char* ie[] = {"VK_KHR_portability_enumeration"};
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo = &app;
        ici.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR; ici.enabledExtensionCount = 1; ici.ppEnabledExtensionNames = ie;
        VkInstance instance{}; CHECK(vkCreateInstance(&ici, nullptr, &instance));
        uint32_t n = 1; CHECK(vkEnumeratePhysicalDevices(instance, &n, &gpu));
        VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(gpu, &props); std::printf("GPU: %s\n", props.deviceName);
        uint32_t qn = 0; vkGetPhysicalDeviceQueueFamilyProperties(gpu, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qps(qn); vkGetPhysicalDeviceQueueFamilyProperties(gpu, &qn, qps.data());
        uint32_t qi = 0; while (qi < qn && !(qps[qi].queueFlags & VK_QUEUE_COMPUTE_BIT)) ++qi;
        if (qi == qn) throw std::runtime_error("No compute queue");
        float priority = 1.0f; VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qci.queueFamilyIndex = qi; qci.queueCount = 1; qci.pQueuePriorities = &priority;
        VkPhysicalDeviceBufferDeviceAddressFeatures bda{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES}; bda.bufferDeviceAddress = VK_TRUE;
        VkPhysicalDeviceAccelerationStructureFeaturesKHR af{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR}; af.accelerationStructure = VK_TRUE; af.pNext = &bda;
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rf{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR}; rf.rayTracingPipeline = VK_TRUE; rf.pNext = &af;
        const char* de[] = {"VK_KHR_portability_subset", "VK_KHR_acceleration_structure", "VK_KHR_ray_tracing_pipeline", "VK_KHR_deferred_host_operations"};
        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dci.pNext = &rf; dci.queueCreateInfoCount = 1;
        dci.pQueueCreateInfos = &qci; dci.enabledExtensionCount = 4; dci.ppEnabledExtensionNames = de;
        CHECK(vkCreateDevice(gpu, &dci, nullptr, &dev)); vkGetDeviceQueue(dev, qi, 0, &queue);
#define LOAD(v, name) v = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(dev, #name)); if (!v) throw std::runtime_error(#name)
        LOAD(createAS, vkCreateAccelerationStructureKHR); LOAD(asSizes, vkGetAccelerationStructureBuildSizesKHR);
        LOAD(buildAS, vkCmdBuildAccelerationStructuresKHR); LOAD(asAddress, vkGetAccelerationStructureDeviceAddressKHR);
        LOAD(createRT, vkCreateRayTracingPipelinesKHR); LOAD(groupHandles, vkGetRayTracingShaderGroupHandlesKHR);
        LOAD(traceRays, vkCmdTraceRaysKHR);
#undef LOAD
        VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pci.queueFamilyIndex = qi;
        CHECK(vkCreateCommandPool(dev, &pci, nullptr, &pool));

        const float vertices[] = {-1,-1,0, 1,-1,0, 0,1,0};
        auto vb = buffer(sizeof(vertices), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, vertices);
        std::vector<VkAccelerationStructureKHR> blases;
        std::vector<VkAccelerationStructureInstanceKHR> instances(4);
        const uint32_t masks[] = {1u, 2u, 4u, 8u};
        for (uint32_t i = 0; i < 4; ++i) {
            VkAccelerationStructureGeometryKHR geom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
            geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            // Match production AS construction: non-opaque triangles request
            // one any-hit invocation per primitive; the blocker is opaque.
            geom.flags = i == 3 ? VK_GEOMETRY_OPAQUE_BIT_KHR
                                : VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
            auto& tri = geom.geometry.triangles; tri.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            tri.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT; tri.vertexData.deviceAddress = vb.address;
            tri.vertexStride = 12; tri.maxVertex = 2; tri.indexType = VK_INDEX_TYPE_NONE_KHR;
            auto blas = acceleration(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, geom); blases.push_back(blas);
            VkAccelerationStructureDeviceAddressInfoKHR ai{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR}; ai.accelerationStructure = blas;
            instances[i].transform.matrix[0][0] = instances[i].transform.matrix[1][1] = instances[i].transform.matrix[2][2] = 1.0f;
            instances[i].transform.matrix[2][3] = 0.1f * float(i);
            instances[i].instanceCustomIndex = i; instances[i].mask = masks[i];
            instances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instances[i].accelerationStructureReference = asAddress(dev, &ai);
        }
        auto ib = buffer(sizeof(VkAccelerationStructureInstanceKHR) * instances.size(),
                         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, instances.data());
        VkAccelerationStructureGeometryKHR ig{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        ig.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR; ig.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        ig.geometry.instances.data.deviceAddress = ib.address;
        auto tlas = acceleration(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, ig, uint32_t(instances.size()));

        VkDescriptorSetLayoutBinding bindings[] = {
            {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr}};
        VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; lci.bindingCount = 2; lci.pBindings = bindings;
        VkDescriptorSetLayout layout{}; CHECK(vkCreateDescriptorSetLayout(dev, &lci, nullptr, &layout));
        VkPushConstantRange pcr{VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, 8};
        VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; plci.setLayoutCount = 1; plci.pSetLayouts = &layout;
        plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pcr;
        VkPipelineLayout pl{}; CHECK(vkCreatePipelineLayout(dev, &plci, nullptr, &pl));
        VkShaderModule modules[] = {shader("probes/rt_shadow_flags.rgen.spv"), shader("probes/rt_shadow_flags.rmiss.spv"),
                                    shader("probes/rt_shadow_flags.rchit.spv"), shader("probes/rt_shadow_flags.rahit.spv")};
        VkPipelineShaderStageCreateInfo stages[4]{}; VkRayTracingShaderGroupCreateInfoKHR groups[3]{};
        VkShaderStageFlagBits stageKinds[] = {VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_SHADER_STAGE_MISS_BIT_KHR,
                                               VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, VK_SHADER_STAGE_ANY_HIT_BIT_KHR};
        for (uint32_t i = 0; i < 4; ++i) { stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[i].stage = stageKinds[i]; stages[i].module = modules[i]; stages[i].pName = "main"; }
        for (uint32_t i = 0; i < 3; ++i) {
            groups[i].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            groups[i].type = i == 2 ? VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR : VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            groups[i].generalShader = i < 2 ? i : VK_SHADER_UNUSED_KHR;
            groups[i].closestHitShader = i == 2 ? 2 : VK_SHADER_UNUSED_KHR;
            groups[i].anyHitShader = i == 2 ? 3 : VK_SHADER_UNUSED_KHR;
            groups[i].intersectionShader = VK_SHADER_UNUSED_KHR;
        }
        VkRayTracingPipelineCreateInfoKHR rci{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
        rci.stageCount = 4; rci.pStages = stages; rci.groupCount = 3; rci.pGroups = groups;
        rci.maxPipelineRayRecursionDepth = 1; rci.layout = pl; VkPipeline pipeline{};
        CHECK(createRT(dev, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rci, nullptr, &pipeline));
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
        VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; p2.pNext = &rt; vkGetPhysicalDeviceProperties2(gpu, &p2);
        VkDeviceSize stride = (rt.shaderGroupHandleSize + rt.shaderGroupBaseAlignment - 1) & ~(VkDeviceSize(rt.shaderGroupBaseAlignment) - 1);
        auto sbt = buffer(stride * 7, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR);
        auto base = (sbt.address + rt.shaderGroupBaseAlignment - 1) & ~(VkDeviceAddress(rt.shaderGroupBaseAlignment) - 1);
        auto sbtOffset = base - sbt.address;
        std::vector<char> handles(rt.shaderGroupHandleSize * 3); CHECK(groupHandles(dev, pipeline, 0, 3, handles.size(), handles.data()));
        for (uint32_t i = 0; i < 3; ++i) std::memcpy(static_cast<char*>(sbt.mapped) + sbtOffset + i * stride,
                                                      handles.data() + i * rt.shaderGroupHandleSize, rt.shaderGroupHandleSize);
        for (uint32_t i = 0; i < 4; ++i) std::memcpy(static_cast<char*>(sbt.mapped) + sbtOffset + (3 + i) * stride,
                                                      handles.data() + 2 * rt.shaderGroupHandleSize, rt.shaderGroupHandleSize);
        auto output = buffer(resultBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        VkDescriptorPoolSize ps[] = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};
        VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; dpci.maxSets = 1; dpci.poolSizeCount = 2; dpci.pPoolSizes = ps;
        VkDescriptorPool dp{}; CHECK(vkCreateDescriptorPool(dev, &dpci, nullptr, &dp));
        VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; dsai.descriptorPool = dp; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &layout;
        VkDescriptorSet ds{}; CHECK(vkAllocateDescriptorSets(dev, &dsai, &ds));
        VkWriteDescriptorSetAccelerationStructureKHR aw{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        aw.accelerationStructureCount = 1; aw.pAccelerationStructures = &tlas;
        VkDescriptorBufferInfo obi{output.buffer, 0, resultBytes}; VkWriteDescriptorSet writes[2]{};
        for (uint32_t i = 0; i < 2; ++i) { writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[i].dstSet = ds; writes[i].dstBinding = i; writes[i].descriptorCount = 1; }
        writes[0].pNext = &aw; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[1].pBufferInfo = &obi; vkUpdateDescriptorSets(dev, 2, writes, 0, nullptr);
        VkQueryPoolCreateInfo qpi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO}; qpi.queryType = VK_QUERY_TYPE_TIMESTAMP; qpi.queryCount = 4;
        VkQueryPool timestamps{}; CHECK(vkCreateQueryPool(dev, &qpi, nullptr, &timestamps));
        std::vector<double> baseline, fast;
        struct PushParams { uint32_t rayCount; uint32_t fastFlags; } pushParams{rayCount, 0};
        for (uint32_t rep = 0; rep < repeats + 2; ++rep) {
            std::memset(output.mapped, 0, resultBytes);
            auto cmd = begin(); vkCmdResetQueryPool(cmd, timestamps, 0, 4);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pl, 0, 1, &ds, 0, nullptr);
            vkCmdPushConstants(cmd, pl, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, 8, &pushParams);
            VkStridedDeviceAddressRegionKHR rg{base, stride, stride}, miss{base + stride, stride, stride};
            VkStridedDeviceAddressRegionKHR hit{base + stride * 3, stride, stride * 4}, call{};
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestamps, 0);
            traceRays(cmd, &rg, &miss, &hit, &call, width, height, 1);
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamps, 1);
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestamps, 2);
            pushParams.fastFlags = 1;
            vkCmdPushConstants(cmd, pl, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, 8, &pushParams);
            traceRays(cmd, &rg, &miss, &hit, &call, width, height, 1);
            pushParams.fastFlags = 0;
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamps, 3);
            VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
            submit(cmd);
            uint64_t ticks[4]{}; CHECK(vkGetQueryPoolResults(dev, timestamps, 0, 4, sizeof(ticks), ticks, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT));
            auto* values = static_cast<float*>(output.mapped);
            const float t[] = {0.5f, 0.75f, 0.9f};
            for (uint32_t mode = 0; mode < 2; ++mode) for (uint32_t y = 0; y < height; ++y) for (uint32_t x = 0; x < width; ++x) {
                uint32_t scenario = y;
                bool missExpected = scenario == 1 || scenario == 3;
                bool transmitted = scenario == 1;
                bool compareThroughput = missExpected || scenario == 2;
                // Traversal order and the number of any-hit invocations before
                // an accepted blocker are unspecified; only assert counts for
                // miss rays, where every ignored layer must be visited.
                bool compareAnyHitCount = scenario == 1 || scenario == 3 || scenario == 2;
                uint32_t expectedAH = scenario == 1 ? 2u * rayCount : 0u;
                float* radiance = values + (VkDeviceSize(mode) * width * height + y * width + x) * 4;
                float* throughput = values + (VkDeviceSize(mode + 2) * width * height + y * width + x) * 4;
                float expectedT[3] = {transmitted ? t[0] : 1.0f, transmitted ? t[1] : 1.0f, transmitted ? t[2] : 1.0f};
                for (int c = 0; c < 3; ++c) {
                    float expectedR = missExpected ? expectedT[c] * float(rayCount) : 0.0f;
                    if (std::abs(radiance[c] - expectedR) > 0.01f ||
                        (compareThroughput && std::abs(throughput[c] - expectedT[c]) > 0.01f)) {
                        std::fprintf(stderr, "Mismatch mode=%u scenario=%u pixel=%u channel=%d radiance=%f throughput=%f expected=%f/%f\n",
                                     mode, scenario, x, c, radiance[c], throughput[c], expectedR, expectedT[c]);
                        throw std::runtime_error("Shadow result mismatch");
                    }
                }
                if ((compareAnyHitCount && uint32_t(radiance[3]) != expectedAH) || uint32_t(throughput[3]) != scenario) {
                    std::fprintf(stderr, "Hit-count mismatch mode=%u scenario=%u pixel=%u anyhits=%.0f expected=%u tag=%.0f\n",
                                 mode, scenario, x, radiance[3], expectedAH, throughput[3]);
                    throw std::runtime_error("Shadow hit-count mismatch");
                }
            }
            if (rep >= 2) {
                double ns = props.limits.timestampPeriod;
                baseline.push_back(double(ticks[1] - ticks[0]) * ns / 1e6);
                fast.push_back(double(ticks[3] - ticks[2]) * ns / 1e6);
            }
        }
        std::sort(baseline.begin(), baseline.end()); std::sort(fast.begin(), fast.end());
        std::printf("RESULT {\"width\":%u,\"height\":%u,\"ray_count\":%u,\"layers\":4,\"repeats\":%u,\"baseline_gpu_ms_median\":%.6f,\"fast_gpu_ms_median\":%.6f,\"baseline_correct\":true,\"fast_correct\":true}\n",
                    width, height, rayCount, repeats, baseline[baseline.size()/2], fast[fast.size()/2]);
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}
