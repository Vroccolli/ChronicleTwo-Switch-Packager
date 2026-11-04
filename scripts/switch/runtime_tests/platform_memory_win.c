#include "runtime/platform_memory.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdlib.h>

void ctRuntimeTestInit(void) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}
void *ctRuntimeAllocatePages(size_t bytes) { return VirtualAlloc(NULL, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); }
void ctRuntimeFreePages(void *pointer) { VirtualFree(pointer, 0, MEM_RELEASE); }
uint32_t ctRuntimeProtectPages(void *pointer, size_t bytes, int writable) {
    DWORD previous;
    return VirtualProtect(pointer, bytes, writable ? PAGE_READWRITE : PAGE_NOACCESS, &previous) ? 0 : GetLastError();
}
uint32_t ctRuntimeQueryPermission(const void *pointer, unsigned *permission) {
    MEMORY_BASIC_INFORMATION info;
    if (!VirtualQuery(pointer, &info, sizeof info)) return GetLastError();
    *permission = info.Protect == PAGE_NOACCESS ? 0 : (info.Protect == PAGE_READWRITE ? 3 : 1);
    return 0;
}
