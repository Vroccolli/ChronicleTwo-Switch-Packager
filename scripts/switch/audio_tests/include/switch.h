#pragma once
using u64=uint64_t; using u32=uint32_t; using Result=uint32_t;
struct AudioOutBuffer { AudioOutBuffer*next; void*buffer; u64 buffer_size,data_size,data_offset; };
#define R_FAILED(r) ((r)!=0)
#define KERNELRESULT(x) 117
Result audoutInitialize(); Result audoutStartAudioOut(); Result audoutStopAudioOut();
void audoutExit(); Result audoutAppendAudioOutBuffer(AudioOutBuffer*);
Result audoutWaitPlayFinish(AudioOutBuffer**,u32*,u64);
