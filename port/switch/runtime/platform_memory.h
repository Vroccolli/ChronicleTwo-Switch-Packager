#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void *ctRuntimeAllocatePages(size_t bytes);
void ctRuntimeFreePages(void *pointer);
uint32_t ctRuntimeProtectPages(void *pointer, size_t bytes, int writable);
uint32_t ctRuntimeQueryPermission(const void *pointer, unsigned *permission);
size_t ctRuntimeLiveMappings(void);
size_t ctRuntimeReleaseMappings(void);
#ifdef __cplusplus
}
#endif
