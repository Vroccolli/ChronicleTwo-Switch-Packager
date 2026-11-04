#pragma once
#include <cstddef>
#include <cstdint>
#include "platform/memory.hpp"

// AAPCS64 callee registers, return address, stack, floating control/status and v8-v15.
struct alignas(16) CtFiberRegisters {
    std::uint64_t gpr[12]{}; // x19..x30
    std::uint64_t sp = 0;
    std::uint64_t fpcr = 0;
    std::uint64_t fpsr = 0;
    std::uint64_t reserved = 0;
    std::uint64_t vectors[16]{}; // q8..q15, two words each
};
static_assert(sizeof(CtFiberRegisters) == 256);
static_assert(offsetof(CtFiberRegisters, sp) == 96);
static_assert(offsetof(CtFiberRegisters, fpcr) == 104);
static_assert(offsetof(CtFiberRegisters, fpsr) == 112);
static_assert(offsetof(CtFiberRegisters, vectors) == 128);

struct CtFiberContext {
    CtFiberRegisters registers;
    ArenaMemory stack;
};
extern "C" void ctFiberSwap(CtFiberRegisters *previous, const CtFiberRegisters *next);
extern "C" void ctFiberStart();
void CtFiberCreate(CtFiberContext &context, void (*entry)(), std::size_t stack_size);
void CtFiberDestroy(CtFiberContext &context);
