#include "crash/platform.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
uint64_t ctCrashThreadId(void) { return GetCurrentThreadId(); }
uintptr_t ctCrashModuleBase(void) { return (uintptr_t)GetModuleHandleW(NULL); }
uintptr_t ctCrashCodeEnd(void) {
    uintptr_t base = ctCrashModuleBase();
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    return base + nt->OptionalHeader.BaseOfCode + nt->OptionalHeader.SizeOfCode;
}
size_t ctCrashFrames(uint64_t *frames, size_t capacity) {
    void *addresses[128];
    USHORT count = CaptureStackBackTrace(0, (DWORD)(capacity < 128 ? capacity : 128), addresses, NULL);
    for (size_t i = 0; i < count; ++i) frames[i] = (uintptr_t)addresses[i];
    return count;
}
