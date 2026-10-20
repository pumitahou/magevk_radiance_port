#include <vulkan/vulkan.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#define CHECK(expr)                                                                                 \
    do {                                                                                            \
        VkResult result_ = (expr);                                                                  \
        if (result_ != VK_SUCCESS) {                                                                \
            std::fprintf(stderr, "FAIL %s VkResult=%d\n", #expr, int(result_));                    \
            throw std::runtime_error(#expr);                                                        \
        }                                                                                           \
    } while (0)

struct Buffer {
    VkBuffer handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

static VkInstance instance;
static VkPhysicalDevice physicalDevice;
static VkDevice device;
static VkQueue queue;
static VkCommandPool commandPool;
static std::thread worker;
static std::atomic<bool> stopWorker{false};
static std::atomic<uint64_t> churnedBuffers{0};
static std::atomic<int> workerFailure{VK_SUCCESS};

static uint32_t memoryType(uint32_t allowed, VkMemoryPropertyFlags required = 0) {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if ((allowed & (1u << i)) &&
            (properties.memoryTypes[i].propertyFlags & required) == required) return i;
    }
    throw std::runtime_error("No compatible Vulkan memory type");
}

static Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool addressable,
                           VkMemoryPropertyFlags requiredMemory = 0) {
    Buffer result{};
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage | (addressable ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0);
    CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &result.handle));

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, result.handle, &requirements);
    VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
    flags.flags = addressable ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.pNext = addressable ? &flags : nullptr;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType(requirements.memoryTypeBits, requiredMemory);
    VkResult allocated = vkAllocateMemory(device, &allocation, nullptr, &result.memory);
    if (allocated != VK_SUCCESS) {
        vkDestroyBuffer(device, result.handle, nullptr);
        result.handle = VK_NULL_HANDLE;
        CHECK(allocated);
    }
    VkResult bound = vkBindBufferMemory(device, result.handle, result.memory, 0);
    if (bound != VK_SUCCESS) {
        vkFreeMemory(device, result.memory, nullptr);
        vkDestroyBuffer(device, result.handle, nullptr);
        result = {};
        CHECK(bound);
    }
    return result;
}

static void destroyBuffer(Buffer& buffer) {
    if (buffer.handle) vkDestroyBuffer(device, buffer.handle, nullptr);
    if (buffer.memory) vkFreeMemory(device, buffer.memory, nullptr);
    buffer = {};
}

static VkShaderModule loadShader(const char* path) {
    FILE* file = std::fopen(path, "rb");
    if (!file) throw std::runtime_error("Cannot open compiled compute shader");
    std::fseek(file, 0, SEEK_END);
    long byteCount = std::ftell(file);
    std::rewind(file);
    if (byteCount <= 0 || byteCount % 4 != 0) {
        std::fclose(file);
        throw std::runtime_error("Invalid SPIR-V size");
    }
    std::vector<uint32_t> words(size_t(byteCount) / 4);
    size_t readCount = std::fread(words.data(), 1, size_t(byteCount), file);
    std::fclose(file);
    if (readCount != size_t(byteCount)) throw std::runtime_error("Cannot read compiled compute shader");

    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = size_t(byteCount);
    info.pCode = words.data();
    VkShaderModule shader;
    CHECK(vkCreateShaderModule(device, &info, nullptr, &shader));
    return shader;
}

static void churnAddressableBuffers(uint32_t lifetimeMicros, uint32_t bufferBytes) {
    while (!stopWorker.load(std::memory_order_relaxed)) {
        Buffer buffer{};
        try {
            buffer = createBuffer(bufferBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
        } catch (...) {
            workerFailure.store(VK_ERROR_OUT_OF_HOST_MEMORY, std::memory_order_relaxed);
            stopWorker.store(true, std::memory_order_relaxed);
            return;
        }
        // This address is deliberately never supplied to a descriptor, push
        // constant, shader, or command. Only the live counter address is used.
        VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        addressInfo.buffer = buffer.handle;
        (void)vkGetBufferDeviceAddress(device, &addressInfo);
        if (lifetimeMicros) std::this_thread::sleep_for(std::chrono::microseconds(lifetimeMicros));
        destroyBuffer(buffer);
        churnedBuffers.fetch_add(1, std::memory_order_relaxed);
    }
}

int main(int argc, char** argv) {
    try {
        if (argc != 5) throw std::runtime_error("usage: resource_lifetime_stress shader.spv seconds buffer_bytes lifetime_us");
        const std::string shaderPath = argv[1];
        const uint32_t seconds = uint32_t(std::stoul(argv[2]));
        const uint32_t bufferBytes = uint32_t(std::stoul(argv[3]));
        const uint32_t lifetimeMicros = uint32_t(std::stoul(argv[4]));
        if (!seconds || seconds > 300 || bufferBytes < 256 || bufferBytes > 1048576)
            throw std::runtime_error("Stress limits exceeded");

        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "Radiance BDA resource lifetime probe";
        app.apiVersion = VK_API_VERSION_1_2;
        const char* instanceExtensions[] = {"VK_KHR_portability_enumeration"};
        VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instanceInfo.pApplicationInfo = &app;
        instanceInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        instanceInfo.enabledExtensionCount = 1;
        instanceInfo.ppEnabledExtensionNames = instanceExtensions;
        CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance));

        uint32_t physicalCount = 0;
        CHECK(vkEnumeratePhysicalDevices(instance, &physicalCount, nullptr));
        if (!physicalCount) throw std::runtime_error("No Vulkan physical device");
        std::vector<VkPhysicalDevice> physicalDevices(physicalCount);
        CHECK(vkEnumeratePhysicalDevices(instance, &physicalCount, physicalDevices.data()));
        physicalDevice = physicalDevices.front();
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        std::printf("GPU: %s\n", properties.deviceName);

        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, families.data());
        uint32_t family = 0;
        while (family < familyCount && !(families[family].queueFlags & VK_QUEUE_COMPUTE_BIT)) ++family;
        if (family == familyCount) throw std::runtime_error("No Vulkan compute queue");

        VkPhysicalDeviceBufferDeviceAddressFeatures supportedAddress{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
        VkPhysicalDeviceFeatures2 supported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        supported.pNext = &supportedAddress;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &supported);
        if (!supportedAddress.bufferDeviceAddress) throw std::runtime_error("Buffer device address is unsupported");

        float priority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        VkPhysicalDeviceBufferDeviceAddressFeatures enabledAddress{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
        enabledAddress.bufferDeviceAddress = VK_TRUE;
        const char* deviceExtensions[] = {"VK_KHR_portability_subset"};
        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceInfo.pNext = &enabledAddress;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = 1;
        deviceInfo.ppEnabledExtensionNames = deviceExtensions;
        CHECK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device));
        vkGetDeviceQueue(device, family, 0, &queue);

        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = family;
        CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool));

        Buffer counter = createBuffer(sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        void* mapped = nullptr;
        CHECK(vkMapMemory(device, counter.memory, 0, VK_WHOLE_SIZE, 0, &mapped));
        std::memset(mapped, 0, sizeof(uint32_t));
        VkBufferDeviceAddressInfo counterAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        counterAddressInfo.buffer = counter.handle;
        VkDeviceAddress counterAddress = vkGetBufferDeviceAddress(device, &counterAddressInfo);
        if (!counterAddress) throw std::runtime_error("Live counter has no device address");

        VkShaderModule shader = loadShader(shaderPath.c_str());
        VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = shader;
        stage.pName = "main";
        VkPushConstantRange pushRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(counterAddress)};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;
        VkPipelineLayout layout;
        CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout));
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = stage;
        pipelineInfo.layout = layout;
        VkPipeline pipeline;
        CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
        vkDestroyShaderModule(device, shader, nullptr);

        std::atomic<uint64_t> dispatches{0};
        std::atomic<bool> running{true};
        worker = std::thread([&] { churnAddressableBuffers(lifetimeMicros, bufferBytes); });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        while (std::chrono::steady_clock::now() < deadline && !workerFailure.load(std::memory_order_relaxed)) {
            std::vector<VkCommandBuffer> batch;
            batch.reserve(64);
            for (uint32_t i = 0; i < 64; ++i) {
                VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                allocation.commandPool = commandPool;
                allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocation.commandBufferCount = 1;
                VkCommandBuffer command;
                CHECK(vkAllocateCommandBuffers(device, &allocation, &command));
                VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                CHECK(vkBeginCommandBuffer(command, &begin));
                vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
                vkCmdPushConstants(command, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                   sizeof(counterAddress), &counterAddress);
                vkCmdDispatch(command, 64, 1, 1);
                VkMemoryBarrier hostReadBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                hostReadBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                hostReadBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
                vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &hostReadBarrier, 0, nullptr, 0, nullptr);
                CHECK(vkEndCommandBuffer(command));
                batch.push_back(command);
            }
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = uint32_t(batch.size());
            submit.pCommandBuffers = batch.data();
            CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
            CHECK(vkQueueWaitIdle(queue));
            vkFreeCommandBuffers(device, commandPool, uint32_t(batch.size()), batch.data());
            dispatches.fetch_add(batch.size(), std::memory_order_relaxed);
        }
        stopWorker.store(true, std::memory_order_relaxed);
        worker.join();
        if (workerFailure.load(std::memory_order_relaxed) != VK_SUCCESS)
            throw std::runtime_error("Background buffer churn failed");
        CHECK(vkDeviceWaitIdle(device));

        uint32_t observed = *static_cast<uint32_t*>(mapped);
        uint32_t expected = uint32_t(dispatches.load(std::memory_order_relaxed) * 64ull * 64ull);
        if (observed != expected) throw std::runtime_error("Compute counter mismatch");

        std::printf("STRESS_RESULT {\"dispatches\":%llu,\"churned_bda_buffers\":%llu,\"counter\":%u,\"counter_matches\":true}\n",
                    static_cast<unsigned long long>(dispatches.load()),
                    static_cast<unsigned long long>(churnedBuffers.load()), observed);
        vkUnmapMemory(device, counter.memory);
        destroyBuffer(counter);
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, layout, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        return 0;
    } catch (const std::exception& error) {
        stopWorker.store(true, std::memory_order_relaxed);
        if (worker.joinable()) worker.join();
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        if (device) vkDeviceWaitIdle(device);
        if (device) vkDestroyDevice(device, nullptr);
        if (instance) vkDestroyInstance(instance, nullptr);
        return 1;
    }
}
