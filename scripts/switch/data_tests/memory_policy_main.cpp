#include "gfx/memory_policy.hpp"
#include <cstdio>
int main() {
    VkPhysicalDeviceMemoryProperties p{};
    p.memoryHeapCount=1; p.memoryHeaps[0]={2ull<<30,VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    p.memoryTypeCount=2;
    p.memoryTypes[0]={VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT,0};
    p.memoryTypes[1]=p.memoryTypes[0];
    int cases=0;
    auto check=[&](bool value) { ++cases; if(!value) {std::fprintf(stderr,"policy case %d failed\n",cases);return false;}return true; };
    auto select=[&](uint32_t bits=1,VkMemoryPropertyFlags flags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        uint32_t vendor=0x10de,VkPhysicalDeviceType kind=VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        return ct::gfx::SharedLocalType(p,bits,flags,vendor,kind);
    };
    if(!check(select()==0) || !check(select(2)==1) || !check(!select(0)) ||
       !check(!select(1,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) ||
       !check(!select(1,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) ||
       !check(!select(1,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,0x1002)) ||
       !check(!select(1,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,0x10de,VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU))) return 1;
    p.memoryHeaps[0].flags=0;if(!check(!select()))return 1;
    p.memoryHeaps[0].flags=VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
    p.memoryTypes[0].heapIndex=1;if(!check(!select()))return 1;
    p.memoryTypes[0].heapIndex=0;p.memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    if(!check(!select()))return 1;
    p.memoryTypeCount=0;if(!check(!select()))return 1;
    std::printf("%d shared-heap policy checks passed\n",cases);
}
