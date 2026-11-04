#include <eekernel.h>
#include "platform/arena.hpp"
#include "platform/clock.hpp"
#include "runtime/platform_memory.h"
#include "sce/threads.hpp"
#include "log/log.hpp"

#include <algorithm>
#include <cfenv>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

extern "C" void ctProbeProgress(const char *message);
extern "C" unsigned ctFiberAbiCheck(void (*yield)());

namespace {
constexpr int kPriority = 10;
std::string g_trace;
int g_counts[2]{};
int g_stack_errors = 0;
int g_round_errors = 0;
int g_once = 0;
std::thread::id g_os_thread;
struct Worker { int index; int rounding; };
ThreadParam Param(void (*entry)(void *), int priority) {
    ThreadParam value{};
    value.entry = entry;
    value.initPriority = priority;
    return value;
}
void Yield() { RotateThreadReadyQueue(kPriority); }
void WorkerEntry(void *arg) {
    const auto worker = *static_cast<Worker *>(arg);
    alignas(16) volatile std::uint64_t canary[8];
    for (int i = 0; i < 8; ++i) canary[i] = 0x1234567800000000ull + worker.index * 16 + i;
    if (reinterpret_cast<std::uintptr_t>(&canary) % 16) ++g_stack_errors;
    if (std::fesetround(worker.rounding)) ++g_round_errors;
    for (;;) {
        if (std::this_thread::get_id() != g_os_thread) ++g_stack_errors;
        for (int i = 0; i < 8; ++i)
            if (canary[i] != 0x1234567800000000ull + worker.index * 16 + i) ++g_stack_errors;
        if (std::fegetround() != worker.rounding) ++g_round_errors;
        ++g_counts[worker.index];
        g_trace += worker.index ? 'B' : 'A';
        RotateThreadReadyQueue(kPriority);
    }
}
void Once(void *) { ++g_once; }
void Waiter(void *arg) {
    g_trace += 'w';
    WaitSema(*static_cast<int *>(arg));
    g_trace += 'W';
}
bool Zeroed(const unsigned char *pointer, std::size_t bytes) {
    return std::all_of(pointer, pointer + bytes, [](unsigned char value) { return value == 0; });
}
void Result(const char *label, bool pass) {
    std::printf("%-28s %s\n", label, pass ? "PASS" : "FAIL");
    CT_LOG_FILE_INFO("runtime check {}: {}", label, pass ? "PASS" : "FAIL");
}
}

extern "C" int ctSwitchRuntimeChecks() {
    LogInit({.level_override = "debug"});
    LogConfig config;
    config.console = true;
    LogApplyConfig(config);
    ctProbeProgress("Checking native arena memory...");
    bool layout = true, guards = true;
    for (std::size_t bytes : {std::size_t(0), std::size_t(1000), std::size_t(3) << 20}) {
        auto memory = ArenaMemoryMap(bytes, 0x10000);
        unsigned permission = 99;
        layout &= memory.capacity >= bytes && reinterpret_cast<std::uintptr_t>(memory.base) % 64 == 0;
        layout &= IsAbove4GiB(memory.base) && static_cast<std::size_t>(memory.base - memory.map) >= 0x10000;
        const auto usable = memory.map_size - ArenaMemoryPageSize();
        std::memset(memory.map, 0xa5, usable);
        auto *original = memory.base;
        ArenaMemoryZero(memory);
        layout &= memory.base == original && Zeroed(memory.map, usable);
        std::memset(memory.map, 0x5a, usable);
        ArenaMemoryZeroByRemap(memory);
        layout &= memory.base == original && Zeroed(memory.map, usable);
        guards &= ctRuntimeQueryPermission(ArenaMemoryGuard(memory), &permission) == 0 && permission == 0;
        guards &= ctRuntimeQueryPermission(memory.base, &permission) == 0 && permission == 3;
        ArenaMemoryUnmap(memory);
        guards &= memory.map == nullptr && memory.map_size == 0;
    }
    static char owner;
    auto *small = ArenaBlock(&owner, 1000);
    std::memset(small, 0x77, 1000);
    bool owners = ArenaBlock(&owner, 500) == small && small[0] == 0x77;
    auto *grown = ArenaBlock(&owner, 0x20000);
    owners &= grown != small && Zeroed(small, 1000) && ArenaContains(small) && ArenaContains(grown);
    grown[0] = 4;
    ArenaClearAll();
    owners &= grown[0] == 0 && ArenaBlock(&owner, 1000) == grown;

    ctProbeProgress("Switching cooperative contexts (1000 rounds)...");
    KzThreadsSetMainPriority(kPriority);
    ClockSetUnbounded(true);
    ClockReset();
    g_os_thread = std::this_thread::get_id();
    g_trace.clear();
    g_trace.reserve(4000);
    g_counts[0] = g_counts[1] = g_stack_errors = g_round_errors = 0;
    const int old_rounding = std::fegetround();
    std::fesetround(FE_TONEAREST);
    Worker wa{0, FE_DOWNWARD}, wb{1, FE_UPWARD};
    auto pa = Param(WorkerEntry, kPriority), pb = Param(WorkerEntry, kPriority);
    const int a = CreateThread(&pa), b = CreateThread(&pb);
    bool fifo = a > 0 && b > 0 && StartThread(a, &wa) == a && StartThread(b, &wb) == b;
    unsigned abi_errors = 0;
    for (int i = 0; i < 1000; ++i) {
        g_trace += 'M';
        abi_errors += ctFiberAbiCheck(Yield);
        if (std::fegetround() != FE_TONEAREST) ++g_round_errors;
    }
    fifo &= g_counts[0] == 1000 && g_counts[1] == 1000;
    fifo &= g_trace.size() == 3000;
    if (g_trace.size() == 3000)
        for (int i = 0; i < 1000; ++i) fifo &= g_trace.substr(i * 3, 3) == "MAB";
    fifo &= TerminateThread(a) == 0 && TerminateThread(b) == 0 && DeleteThread(a) == 0 && DeleteThread(b) == 0;
    const bool abi = abi_errors == 0 && g_stack_errors == 0 && g_round_errors == 0;
    std::fesetround(old_rounding);

    ctProbeProgress("Checking priorities, restart and semaphores...");
    auto high = Param(Once, kPriority - 5);
    const int once = CreateThread(&high);
    g_once = 0;
    bool priorities = StartThread(once, nullptr) == once && g_once == 1;
    priorities &= StartThread(once, nullptr) == once && g_once == 2 && DeleteThread(once) == 0;
    SemaParam sp{};
    sp.maxCount = 1;
    int sema = CreateSema(&sp);
    auto wp = Param(Waiter, kPriority - 5);
    int waiter = CreateThread(&wp);
    g_trace.clear();
    bool semaphores = StartThread(waiter, &sema) == waiter && g_trace == "w";
    semaphores &= DeleteSema(sema) == -1 && SignalSema(sema) == sema && g_trace == "wW";
    semaphores &= DeleteThread(waiter) == 0 && DeleteSema(sema) == 0;
    sema = CreateSema(&sp);
    waiter = CreateThread(&wp);
    semaphores &= StartThread(waiter, &sema) == waiter;
    semaphores &= TerminateThread(waiter) == 0 && DeleteThread(waiter) == 0 && DeleteSema(sema) == 0;
    KzThreadsSetMainPriority(1);

    Result("arena layout / zeroing", layout);
    Result("kernel guard permissions", guards);
    Result("owner reuse / retirement", owners);
    Result("cooperative FIFO / stress", fifo);
    Result("ARM64 registers / stack / FP", abi);
    Result("priority / restart", priorities);
    Result("semaphore wait / cancel", semaphores);
    const auto live_before_cleanup = ctRuntimeLiveMappings();
    const auto released = ctRuntimeReleaseMappings();
    const bool cleanup = live_before_cleanup == 2 && released == 2 && ctRuntimeLiveMappings() == 0;
    Result("mapping cleanup", cleanup);
    std::printf("ABI errors=%u stack errors=%d FP errors=%d\n", abi_errors, g_stack_errors, g_round_errors);
    LogFlush();
    return (!layout ? 1 : 0) | (!guards ? 2 : 0) | (!owners ? 4 : 0) | (!fifo ? 8 : 0) |
        (!abi ? 16 : 0) | (!priorities ? 32 : 0) | (!semaphores ? 64 : 0) | (!cleanup ? 128 : 0);
}
