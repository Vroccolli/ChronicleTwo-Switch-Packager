#include "crash/platform.h"
#include <switch.h>
#include <unwind.h>
extern char __rodata_start[];
uint64_t ctCrashThreadId(void) {
    u64 id = 0;
    svcGetThreadId(&id, CUR_THREAD_HANDLE);
    return id;
}
uintptr_t ctCrashModuleBase(void) {
    // __start__ is an absolute linker constant (zero), not a relocated address.
    // Resolve the actual .text entry with PC-relative instructions under ASLR.
    uintptr_t base;
    __asm__("adrp %0, _start\n\tadd %0, %0, :lo12:_start" : "=r"(base));
    return base;
}
uintptr_t ctCrashCodeEnd(void) { return (uintptr_t)__rodata_start; }
struct Frames { uint64_t *frames; size_t capacity, count; };
static _Unwind_Reason_Code frame(struct _Unwind_Context *context, void *argument) {
    struct Frames *out = argument;
    if (out->count == out->capacity) return _URC_END_OF_STACK;
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) out->frames[out->count++] = pc;
    return _URC_NO_REASON;
}
size_t ctCrashFrames(uint64_t *frames, size_t capacity) {
    struct Frames out = {frames, capacity, 0};
    _Unwind_Backtrace(frame, &out);
    return out.count;
}
