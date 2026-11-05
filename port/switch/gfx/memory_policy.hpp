#pragma once
#include <vulkan/vulkan_core.h>
#include <optional>

namespace ct::gfx {
// NVK 25.0.7 exposes Tegra's shared RAM as a DEVICE_LOCAL heap with an
// HOST_VISIBLE|HOST_COHERENT|HOST_CACHED type, omitting DEVICE_LOCAL on the
// type itself. Device locality is an allocator preference here; resource
// compatibility and CPU visibility/coherency requirements remain mandatory.
inline std::optional<uint32_t> SharedLocalType(
    const VkPhysicalDeviceMemoryProperties &properties, uint32_t compatible,
    VkMemoryPropertyFlags required, uint32_t vendor, VkPhysicalDeviceType device) {
    if (vendor!=0x10de || device!=VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ||
        required!=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) return std::nullopt;
    constexpr auto host=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i=0;i<properties.memoryTypeCount;++i) {
        const auto &type=properties.memoryTypes[i];
        if ((compatible&(1u<<i)) && (type.propertyFlags&host)==host &&
            type.heapIndex<properties.memoryHeapCount &&
            (properties.memoryHeaps[type.heapIndex].flags&VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)) return i;
    }
    return std::nullopt;
}
}
