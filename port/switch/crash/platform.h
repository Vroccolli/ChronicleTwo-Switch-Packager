#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t ctCrashThreadId(void);
uintptr_t ctCrashModuleBase(void);
uintptr_t ctCrashCodeEnd(void);
size_t ctCrashFrames(uint64_t *frames, size_t capacity);
#ifdef __cplusplus
}
#endif
