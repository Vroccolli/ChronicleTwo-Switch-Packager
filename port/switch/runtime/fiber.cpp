#include "runtime/fiber.hpp"
#include "runtime/platform_memory.h"
#include "kazarov/log.hpp"
#include <limits>

void CtFiberCreate(CtFiberContext &context, void (*entry)(), std::size_t stack_size) {
    const auto page = ArenaMemoryPageSize();
    if (stack_size > std::numeric_limits<std::size_t>::max() - 2 * page)
        kz::Abort("fiber", "stack size overflow");
    stack_size = (stack_size + page - 1) & ~(page - 1);
    context = {};
    context.stack = ArenaMemoryMap(stack_size + page);
    const auto result = ctRuntimeProtectPages(context.stack.base, page, false);
    if (result) kz::Abort("fiber", std::format("lower stack guard failed: 0x{:08x}", result));
    context.registers.sp = reinterpret_cast<std::uintptr_t>(ArenaMemoryGuard(context.stack));
    context.registers.gpr[0] = reinterpret_cast<std::uintptr_t>(entry); // x19, consumed by the trampoline
    context.registers.gpr[11] = reinterpret_cast<std::uintptr_t>(&ctFiberStart); // x30
    asm volatile("mrs %0, fpcr" : "=r"(context.registers.fpcr));
    asm volatile("mrs %0, fpsr" : "=r"(context.registers.fpsr));
}
void CtFiberDestroy(CtFiberContext &context) {
    if (context.stack.map) {
        const auto result = ctRuntimeProtectPages(context.stack.base, ArenaMemoryPageSize(), true);
        if (result) kz::Abort("fiber", std::format("lower stack guard restoration failed: 0x{:08x}", result));
        ArenaMemoryUnmap(context.stack);
    }
    context = {};
}
extern "C" [[noreturn]] void ctFiberUnexpectedReturn() {
    kz::Abort("fiber", "scheduler entry returned without selecting another context");
}
