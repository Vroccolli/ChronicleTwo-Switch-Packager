#include <gtest/gtest.h>
#include <limits>
#include "platform/memory.hpp"
#include "runtime/platform_memory.h"

TEST(SwitchMemoryAdapter, QueriesRealGuardAndRestoresItOnRelease) {
    ArenaMemory memory = ArenaMemoryMap(1000, 37);
    unsigned permission = 99;
    EXPECT_EQ(ctRuntimeQueryPermission(memory.base, &permission), 0U);
    EXPECT_EQ(permission, 3U);
    EXPECT_EQ(ctRuntimeQueryPermission(ArenaMemoryGuard(memory), &permission), 0U);
    EXPECT_EQ(permission, 0U);
    ArenaMemoryUnmap(memory);
    EXPECT_EQ(memory.map, nullptr);
    EXPECT_EQ(memory.map_size, 0U);
    EXPECT_EQ(memory.base, nullptr);
}
TEST(SwitchMemoryAdapter, RejectsSizeOverflowBeforeAllocating) {
    EXPECT_DEATH(ArenaMemoryMap(std::numeric_limits<std::size_t>::max()), "allocation size overflow");
    EXPECT_DEATH(ArenaMemoryMap(4096, std::numeric_limits<std::size_t>::max()), "allocation size overflow");
}
TEST(SwitchMemoryAdapter, FinalCleanupReleasesOutstandingMappings) {
    EXPECT_EQ(ctRuntimeLiveMappings(), 0U);
    auto first = ArenaMemoryMap(1000);
    auto second = ArenaMemoryMap(8192);
    EXPECT_EQ(ctRuntimeLiveMappings(), 2U);
    EXPECT_EQ(ctRuntimeReleaseMappings(), 2U);
    EXPECT_EQ(ctRuntimeLiveMappings(), 0U);
    auto next = ArenaMemoryMap(64);
    ArenaMemoryUnmap(next);
    EXPECT_EQ(ctRuntimeLiveMappings(), 0U);
}
