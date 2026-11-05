#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
using Uint8=uint8_t;
#define SDL_AUDIO_DRIVER_SWITCH 1
#define SDL_AUDIO_S16LE 16
void *TestAlloc(size_t,size_t);
void TestFree(void*);
#define SDL_malloc(n) TestAlloc(n,16)
#define SDL_calloc(n,s) TestCalloc(n,s)
#define SDL_free TestFree
#define SDL_memcpy memcpy
#define SDL_memset memset
void *TestCalloc(size_t,size_t);
bool SDL_SetError(const char*,...);
struct SDL_AtomicInt { int value; };
inline int SDL_GetAtomicInt(SDL_AtomicInt*p){return p->value;}
