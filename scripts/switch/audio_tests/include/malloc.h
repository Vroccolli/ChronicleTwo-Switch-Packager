#pragma once
#include <stddef.h>
void *TestAlloc(size_t,size_t);
void TestFree(void*);
#define memalign(a,n) TestAlloc(n,a)
#define free TestFree
