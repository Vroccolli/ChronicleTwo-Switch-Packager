#include "gfx/requirements.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance, const char *);
extern "C" void ctProbeProgress(const char *);
namespace {
VkInstance instance = VK_NULL_HANDLE;
FILE *logfile = nullptr;
void Log(const std::string &line) {
    std::puts(line.c_str());
    if (logfile) { std::fprintf(logfile, "%s\n", line.c_str()); std::fflush(logfile); }
}
template<class T> T Function(const char *name) {
    auto fn = reinterpret_cast<T>(vkGetInstanceProcAddr(instance, name));
    if (!fn) throw std::runtime_error(std::string("Missing driver entrypoint: ") + name);
    return fn;
}
void Check(VkResult result, const char *operation) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string(operation) + ": " + std::to_string(result));
}
}
// The unchanged engine requirements source calls these ordinary Vulkan entry
// points. Dispatch to the static ICD without assuming a desktop Vulkan loader.
extern "C" VkResult vkEnumerateDeviceExtensionProperties(VkPhysicalDevice d, const char *l, uint32_t *n, VkExtensionProperties *p) {
    return Function<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties")(d,l,n,p);
}
extern "C" void vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice d, VkFormat f, VkFormatProperties *p) {
    Function<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties")(d,f,p);
}
extern "C" void vkGetPhysicalDeviceProperties(VkPhysicalDevice d, VkPhysicalDeviceProperties *p) {
    Function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(d,p);
}
extern "C" void vkGetPhysicalDeviceFeatures2(VkPhysicalDevice d, VkPhysicalDeviceFeatures2 *p) {
    Function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(d,p);
}
extern "C" void vkGetPhysicalDeviceProperties2(VkPhysicalDevice d, VkPhysicalDeviceProperties2 *p) {
    Function<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(d,p);
}
extern "C" void vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice d, uint32_t *n, VkQueueFamilyProperties *p) {
    Function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties")(d,n,p);
}
extern "C" VkResult vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice d, uint32_t q, VkSurfaceKHR s, VkBool32 *p) {
    return Function<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>("vkGetPhysicalDeviceSurfaceSupportKHR")(d,q,s,p);
}

extern "C" int ctVulkanProbe() {
    std::error_code error;
    std::filesystem::create_directories("sdmc:/switch/ChronicleTwo-PC-Switch/vulkan-probe", error);
    logfile = std::fopen("sdmc:/switch/ChronicleTwo-PC-Switch/vulkan-probe/result.txt", "wb");
    int failures = 0;
    VkDevice device = VK_NULL_HANDLE;
    bool submitted = false, finished = false;
    try {
        setenv("NVK_I_WANT_A_BROKEN_VULKAN_DRIVER", "1", 1);
        ctProbeProgress("Creating the experimental Vulkan driver instance...");
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "ChronicleTwo capability probe"; app.apiVersion = VK_API_VERSION_1_3;
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; info.pApplicationInfo = &app;
        Check(Function<PFN_vkCreateInstance>("vkCreateInstance")(&info,nullptr,&instance),"vkCreateInstance");
        uint32_t count = 0;
        auto enumerate = Function<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
        Check(enumerate(instance,&count,nullptr),"enumerate GPU count");
        if (!count) throw std::runtime_error("No GPU exposed by driver");
        std::vector<VkPhysicalDevice> physical(count);
        Check(enumerate(instance,&count,physical.data()),"enumerate GPUs");
        auto gpu = physical.front();
        VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(gpu,&properties);
        Log(std::string("GPU: ") + properties.deviceName);
        Log("API: " + std::to_string(VK_API_VERSION_MAJOR(properties.apiVersion)) + "." + std::to_string(VK_API_VERSION_MINOR(properties.apiVersion)));
        const auto caps = gfx::detail::QueryDeviceCaps(gpu,VK_NULL_HANDLE);
        const auto missing = gfx::detail::MissingRequirements(caps,true);
        Log(std::string("Engine offscreen requirements: ") + (missing.empty() ? "PASS" : "FAIL"));
        for (const auto &reason : missing) Log("  missing: " + reason);
        if (!missing.empty()) failures |= 2;
        uint32_t n = 0; vkGetPhysicalDeviceQueueFamilyProperties(gpu,&n,nullptr);
        std::vector<VkQueueFamilyProperties> queues(n); vkGetPhysicalDeviceQueueFamilyProperties(gpu,&n,queues.data());
        uint32_t family = 0;
        while (family < n && !(queues[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) ++family;
        if (family == n) throw std::runtime_error("No graphics queue");
        float priority = 1;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex=family; queueInfo.queueCount=1; queueInfo.pQueuePriorities=&priority;
        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; deviceInfo.queueCreateInfoCount=1; deviceInfo.pQueueCreateInfos=&queueInfo;
        ctProbeProgress("Creating GPU device and testing a fill/readback...");
        Check(Function<PFN_vkCreateDevice>("vkCreateDevice")(gpu,&deviceInfo,nullptr,&device),"vkCreateDevice");
        auto getDevice = Function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
        auto load = [&](const char *name) { auto fn=getDevice(device,name); if (!fn) throw std::runtime_error(std::string("Missing ")+name); return fn; };
#define DEV(name) reinterpret_cast<PFN_##name>(load(#name))
        VkQueue queue{}; DEV(vkGetDeviceQueue)(device,family,0,&queue);
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; bufferInfo.size=4096; bufferInfo.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VkBuffer buffer{}; Check(DEV(vkCreateBuffer)(device,&bufferInfo,nullptr,&buffer),"vkCreateBuffer");
        VkMemoryRequirements requirements{}; DEV(vkGetBufferMemoryRequirements)(device,buffer,&requirements);
        VkPhysicalDeviceMemoryProperties memoryProperties{};
        Function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(gpu,&memoryProperties);
        uint32_t type=0;
        while (type<memoryProperties.memoryTypeCount && (!(requirements.memoryTypeBits&(1u<<type)) ||
            !(memoryProperties.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))) ++type;
        if (type==memoryProperties.memoryTypeCount) throw std::runtime_error("No host-visible buffer memory");
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; alloc.allocationSize=requirements.size; alloc.memoryTypeIndex=type;
        VkDeviceMemory memory{}; Check(DEV(vkAllocateMemory)(device,&alloc,nullptr,&memory),"vkAllocateMemory");
        Check(DEV(vkBindBufferMemory)(device,buffer,memory,0),"vkBindBufferMemory");
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; poolInfo.queueFamilyIndex=family;
        VkCommandPool pool{}; Check(DEV(vkCreateCommandPool)(device,&poolInfo,nullptr,&pool),"vkCreateCommandPool");
        VkCommandBufferAllocateInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; cmdInfo.commandPool=pool; cmdInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cmdInfo.commandBufferCount=1;
        VkCommandBuffer cmd{}; Check(DEV(vkAllocateCommandBuffers)(device,&cmdInfo,&cmd),"vkAllocateCommandBuffers");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; Check(DEV(vkBeginCommandBuffer)(cmd,&begin),"vkBeginCommandBuffer");
        DEV(vkCmdFillBuffer)(cmd,buffer,0,4096,0xcafebabe);
        // Transfer writes must be made available to host reads before signalling.
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        DEV(vkCmdPipelineBarrier)(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        Check(DEV(vkEndCommandBuffer)(cmd),"vkEndCommandBuffer");
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; VkFence fence{};
        Check(DEV(vkCreateFence)(device,&fenceInfo,nullptr,&fence),"vkCreateFence");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=1; submit.pCommandBuffers=&cmd;
        Check(DEV(vkQueueSubmit)(queue,1,&submit,fence),"vkQueueSubmit"); submitted=true;
        Check(DEV(vkWaitForFences)(device,1,&fence,VK_TRUE,5'000'000'000ull),"GPU fence (5 second limit)"); finished=true;
        void *mapped=nullptr; Check(DEV(vkMapMemory)(device,memory,0,VK_WHOLE_SIZE,0,&mapped),"vkMapMemory");
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE}; range.memory=memory; range.size=VK_WHOLE_SIZE;
        Check(DEV(vkInvalidateMappedMemoryRanges)(device,1,&range),"vkInvalidateMappedMemoryRanges");
        bool correct=true; for (size_t i=0;i<1024;++i) if (static_cast<uint32_t *>(mapped)[i]!=0xcafebabe) correct=false;
        Log(std::string("GPU fill/readback (1024 words): ")+(correct ? "PASS" : "FAIL"));
        if (!correct) failures|=4;
        DEV(vkUnmapMemory)(device,memory);
        DEV(vkDestroyFence)(device,fence,nullptr); DEV(vkDestroyCommandPool)(device,pool,nullptr);
        DEV(vkDestroyBuffer)(device,buffer,nullptr); DEV(vkFreeMemory)(device,memory,nullptr);
#undef DEV
    } catch (const std::exception &exception) { Log(exception.what()); failures|=1; }
    // Never block destroying a device after a timed-out GPU submission.
    if (device && (!submitted || finished)) Function<PFN_vkDestroyDevice>("vkDestroyDevice")(device,nullptr);
    if (instance && (!submitted || finished)) Function<PFN_vkDestroyInstance>("vkDestroyInstance")(instance,nullptr);
    Log("Failure bits: " + std::to_string(failures));
    if (logfile) { std::fclose(logfile); logfile=nullptr; }
    return failures;
}
