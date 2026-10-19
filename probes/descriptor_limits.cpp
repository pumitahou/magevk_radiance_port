#include <vulkan/vulkan.h>
#include <cstdio>
#include <stdexcept>
#include <vector>
#define CHECK(x) do { if ((x) != VK_SUCCESS) throw std::runtime_error(#x); } while (0)

// Headless check of the E09 descriptor layouts against the actual driver and
// validation layer. This does not claim that Minecraft rasterization works.
int main() {
    try {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "Radiance E09 descriptor limits";
        app.apiVersion = VK_API_VERSION_1_2;
        const char *instanceExtensions[] = {"VK_KHR_portability_enumeration"};
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ici.pApplicationInfo = &app;
        ici.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        ici.enabledExtensionCount = 1; ici.ppEnabledExtensionNames = instanceExtensions;
        VkInstance instance; CHECK(vkCreateInstance(&ici, nullptr, &instance));
        uint32_t count = 0; CHECK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
        if (!count) throw std::runtime_error("No GPU");
        std::vector<VkPhysicalDevice> devices(count);
        CHECK(vkEnumeratePhysicalDevices(instance, &count, devices.data()));
        VkPhysicalDevice gpu = devices[0];
        VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dynamic{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT};
        VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
        acceleration.pNext = &dynamic;
        VkPhysicalDeviceVulkan12Features supported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        supported.pNext = &acceleration;
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; features.pNext = &supported;
        vkGetPhysicalDeviceFeatures2(gpu, &features);
        printf("dynamicColorBlendEnable=%u equation=%u writeMask=%u ASUpdateAfterBind=%u\n",
               dynamic.extendedDynamicState3ColorBlendEnable, dynamic.extendedDynamicState3ColorBlendEquation,
               dynamic.extendedDynamicState3ColorWriteMask, acceleration.descriptorBindingAccelerationStructureUpdateAfterBind);
        VkPhysicalDeviceVulkan12Features enabled{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        enabled.bufferDeviceAddress = supported.bufferDeviceAddress;
        enabled.descriptorIndexing = supported.descriptorIndexing;
        enabled.runtimeDescriptorArray = supported.runtimeDescriptorArray;
        enabled.descriptorBindingPartiallyBound = supported.descriptorBindingPartiallyBound;
        enabled.descriptorBindingSampledImageUpdateAfterBind = supported.descriptorBindingSampledImageUpdateAfterBind;
        enabled.descriptorBindingUpdateUnusedWhilePending = supported.descriptorBindingUpdateUnusedWhilePending;
        VkPhysicalDeviceAccelerationStructureFeaturesKHR asEnabled{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
        asEnabled.accelerationStructure = acceleration.accelerationStructure; asEnabled.pNext = &enabled;
        uint32_t queueCount = 0; vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queueCount); vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueCount, queues.data());
        uint32_t family = 0; while (family < queueCount && !(queues[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) ++family;
        if (family == queueCount) throw std::runtime_error("No graphics queue");
        float priority = 1;
        VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qci.queueFamilyIndex = family; qci.queueCount = 1; qci.pQueuePriorities = &priority;
        const char *deviceExtensions[] = {"VK_KHR_portability_subset", "VK_KHR_acceleration_structure", "VK_KHR_deferred_host_operations"};
        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dci.pNext = &asEnabled;
        dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci; dci.enabledExtensionCount = 3; dci.ppEnabledExtensionNames = deviceExtensions;
        VkDevice device; CHECK(vkCreateDevice(gpu, &dci, nullptr, &device));
        VkDescriptorSetLayoutBinding bindings[] = {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };
        constexpr VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        VkDescriptorBindingFlags bindingFlags[] = {flags, flags};
        VkDescriptorSetLayoutBindingFlagsCreateInfo flagInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
        flagInfo.bindingCount = 2; flagInfo.pBindingFlags = bindingFlags;
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.pNext = &flagInfo; layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutInfo.bindingCount = 2; layoutInfo.pBindings = bindings;
        VkDescriptorSetLayoutSupport support{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT};
        vkGetDescriptorSetLayoutSupport(device, &layoutInfo, &support);
        printf("Original 4097-descriptor layout supported=%u\n", support.supported);
        uint32_t low = 0, high = 4096;
        while (low < high) {
            uint32_t candidate = low + (high - low + 1) / 2;
            bindings[0].descriptorCount = candidate;
            vkGetDescriptorSetLayoutSupport(device, &layoutInfo, &support);
            if (support.supported) low = candidate; else high = candidate - 1;
        }
        if (!low) throw std::runtime_error("No supported layout");
        bindings[0].descriptorCount = low;
        vkGetDescriptorSetLayoutSupport(device, &layoutInfo, &support);
        if (!support.supported) throw std::runtime_error("Chosen layout unsupported");
        VkDescriptorSetLayout uiLayout; CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &uiLayout));
        VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, low + 1},
                                          {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1}};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT; poolInfo.maxSets = 2;
        poolInfo.poolSizeCount = 2; poolInfo.pPoolSizes = poolSizes;
        VkDescriptorPool pool; CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool));
        VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool = pool; allocation.descriptorSetCount = 1; allocation.pSetLayouts = &uiLayout;
        VkDescriptorSet set; CHECK(vkAllocateDescriptorSets(device, &allocation, &set));
        VkDescriptorSetLayoutBinding asBinding{0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo asInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        asInfo.bindingCount = 1; asInfo.pBindings = &asBinding;
        VkDescriptorSetLayout asLayout; CHECK(vkCreateDescriptorSetLayout(device, &asInfo, nullptr, &asLayout));
        allocation.pSetLayouts = &asLayout;
        VkDescriptorSet asSet; CHECK(vkAllocateDescriptorSets(device, &allocation, &asSet));
        VkDescriptorSetLayout layouts[] = {uiLayout, asLayout};
        VkPipelineLayoutCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineInfo.setLayoutCount = 2; pipelineInfo.pSetLayouts = layouts;
        VkPipelineLayout pipelineLayout; CHECK(vkCreatePipelineLayout(device, &pipelineInfo, nullptr, &pipelineLayout));
        printf("PASS: texture capacity=%u; UI and static AS sets allocated in shared pool; combined pipeline layout created\n", low);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, asLayout, nullptr);
        vkDestroyDescriptorPool(device, pool, nullptr);
        vkDestroyDescriptorSetLayout(device, uiLayout, nullptr);
        vkDestroyDevice(device, nullptr); vkDestroyInstance(instance, nullptr);
        return 0;
    } catch (const std::exception &error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
