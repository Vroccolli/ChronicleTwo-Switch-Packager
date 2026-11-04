#include "platform/memory.hpp"
#include "runtime/platform_memory.h"
#include "kazarov/log.hpp"

#include <cstring>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <vector>

namespace {
constexpr std::size_t kPage = 0x1000;
struct Mapping { unsigned char *map; std::size_t bytes; };
struct Registry { std::mutex mutex; std::vector<Mapping> mappings; };
Registry &Mappings() {
    // Keep this state alive for the exit hook, after ordinary static objects are destroyed.
    static Registry *registry = [] {
        auto *created = new Registry;
        std::atexit([] { ctRuntimeReleaseMappings(); });
        return created;
    }();
    return *registry;
}
std::size_t Add(std::size_t a, std::size_t b) {
    if (a > std::numeric_limits<std::size_t>::max() - b) kz::Abort("arena", "allocation size overflow");
    return a + b;
}
std::size_t Round(std::size_t value, std::size_t alignment) {
    return Add(value, alignment - 1) & ~(alignment - 1);
}
}

std::size_t ArenaMemoryPageSize() { return kPage; }
ArenaMemory ArenaMemoryMap(std::size_t bytes, std::size_t lead) {
    const auto capacity = Round(bytes ? bytes : 64, 64);
    const auto usable = Round(Add(lead, capacity), kPage);
    const auto total = Add(usable, kPage);
    auto *map = static_cast<unsigned char *>(ctRuntimeAllocatePages(total));
    if (!map) kz::Abort("arena", std::format("cannot allocate {} Switch arena bytes", total));
    std::memset(map, 0, usable);
    const auto result = ctRuntimeProtectPages(map + usable, kPage, false);
    if (result) {
        ctRuntimeFreePages(map);
        kz::Abort("arena", std::format("guard protection failed: 0x{:08x}", result));
    }
    auto &registry = Mappings();
    {
        const std::lock_guard lock(registry.mutex);
        registry.mappings.push_back({map, total});
    }
    return {map, total, map + usable - capacity, capacity, lead};
}
void ArenaMemoryZero(const ArenaMemory &memory) {
    if (memory.map) std::memset(memory.map, 0, memory.map_size - kPage);
}
void ArenaMemoryZeroByRemap(const ArenaMemory &memory) {
    // Horizon heap allocations are physically backed; clear in place without promising page decommit.
    ArenaMemoryZero(memory);
}
const unsigned char *ArenaMemoryGuard(const ArenaMemory &memory) { return memory.base + memory.capacity; }
void ArenaMemoryUnmap(ArenaMemory &memory) {
    if (memory.map) {
        const auto result = ctRuntimeProtectPages(memory.map + memory.map_size - kPage, kPage, true);
        if (result) kz::Abort("arena", std::format("guard restoration failed: 0x{:08x}", result));
        auto &registry = Mappings();
        {
            const std::lock_guard lock(registry.mutex);
            std::erase_if(registry.mappings, [&](const Mapping &entry) { return entry.map == memory.map; });
        }
        ctRuntimeFreePages(memory.map);
    }
    memory = {};
}
extern "C" std::size_t ctRuntimeLiveMappings() {
    auto &registry = Mappings();
    const std::lock_guard lock(registry.mutex);
    return registry.mappings.size();
}
extern "C" std::size_t ctRuntimeReleaseMappings() {
    auto &registry = Mappings();
    const std::lock_guard lock(registry.mutex);
    const auto count = registry.mappings.size();
    for (const auto &entry : registry.mappings) {
        const auto lower = ctRuntimeProtectPages(entry.map, kPage, true);
        if (lower) kz::Abort("arena", std::format("exit lower guard restoration failed: 0x{:08x}", lower));
        const auto result = ctRuntimeProtectPages(entry.map + entry.bytes - kPage, kPage, true);
        if (result) kz::Abort("arena", std::format("exit guard restoration failed: 0x{:08x}", result));
        ctRuntimeFreePages(entry.map);
    }
    registry.mappings.clear();
    return count;
}
