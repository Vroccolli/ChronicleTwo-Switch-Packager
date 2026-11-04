#include "runtime/platform_memory.h"
#include <malloc.h>
#include <stdlib.h>
#include <switch.h>

void *ctRuntimeAllocatePages(size_t bytes) { return memalign(0x1000, bytes); }
void ctRuntimeFreePages(void *pointer) { free(pointer); }
uint32_t ctRuntimeProtectPages(void *pointer, size_t bytes, int writable) {
    return svcSetMemoryPermission(pointer, bytes, writable ? Perm_Rw : Perm_None);
}
uint32_t ctRuntimeQueryPermission(const void *pointer, unsigned *permission) {
    MemoryInfo info;
    u32 page;
    Result result = svcQueryMemory(&info, &page, (u64)pointer);
    if (R_SUCCEEDED(result)) *permission = info.perm;
    return result;
}
