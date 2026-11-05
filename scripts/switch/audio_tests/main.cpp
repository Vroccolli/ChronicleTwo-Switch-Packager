#include <cstdio>
#include <vector>
#include <deque>
#include <map>
#include <stdexcept>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <malloc.h>
#undef free
#define memalign(a,n) TestAlloc(n,a)
#define free TestFree
#include "../../../port/switch/sdl/audio.c"
#undef free
#define CHECK(x) do {++checks;if(!(x))throw std::runtime_error(#x);}while(0)
static int checks=0, allocIndex=0, failAlloc=0, waitMode=0, failStage=0;
static bool initialized=false,started=false;
static std::map<void*,void*> allocations;
struct Packet { AudioOutBuffer* tag; std::vector<Uint8> bytes; };
static std::deque<Packet> queue;
static std::vector<std::vector<Uint8>> played;
void *TestAlloc(size_t n,size_t align){
 if(++allocIndex==failAlloc)return nullptr;
 void* raw=std::malloc(n+align);if(!raw)return nullptr;
 auto aligned=(reinterpret_cast<uintptr_t>(raw)+align-1)&~(align-1);
 void*p=reinterpret_cast<void*>(aligned);allocations[p]=raw;return p;
}
void *TestCalloc(size_t n,size_t s){void*p=TestAlloc(n*s,16);if(p)std::memset(p,0,n*s);return p;}
void TestFree(void*p){if(!p)return;for(auto&packet:queue)CHECK(packet.tag->buffer!=p);auto it=allocations.find(p);CHECK(it!=allocations.end());::free(it->second);allocations.erase(it);}
bool SDL_SetError(const char*,...){return false;}
Result audoutInitialize(){if(failStage==1)return 1;initialized=true;return 0;}
Result audoutStartAudioOut(){CHECK(initialized);CHECK(queue.size()==1);if(failStage==3)return 1;started=true;return 0;}
Result audoutStopAudioOut(){started=false;return 0;}
void audoutExit(){CHECK(!started);queue.clear();initialized=false;}
Result audoutAppendAudioOutBuffer(AudioOutBuffer*b){
 CHECK(initialized);CHECK(reinterpret_cast<uintptr_t>(b->buffer)%4096==0);
 CHECK(b->buffer_size%4096==0);CHECK(b->data_size<=b->buffer_size);
 for(auto&p:queue)CHECK(p.tag!=b);
 if(failStage==2)return 1;
 auto bytes=static_cast<Uint8*>(b->buffer);queue.push_back({b,{bytes,bytes+b->data_size}});return 0;
}
Result audoutWaitPlayFinish(AudioOutBuffer**released,u32*count,u64 timeout){
 CHECK(timeout==100000000);CHECK(started);
 if(waitMode==1){waitMode=0;return KERNELRESULT(TimedOut);}
 if(waitMode==2){waitMode=0;*count=0;return 0;}
 if(waitMode==3)return 1;
 if(waitMode==4){*count=1;*released=nullptr;return 0;}
 CHECK(queue.size()==2);
 Packet p=waitMode==5?queue.back():queue.front();if(waitMode==5){queue.pop_back();waitMode=0;}else{queue.pop_front();}CHECK(std::memcmp(p.bytes.data(),p.tag->buffer,p.bytes.size())==0);
 played.push_back(p.bytes);*released=p.tag;*count=1;return 0;
}
static void reset(){CHECK(allocations.empty());CHECK(queue.empty());allocIndex=failAlloc=waitMode=failStage=0;played.clear();}
int main(){try{
 for(int bytes: {1024,2048,4096,8192}){
 reset();SDL_AudioDevice d;d.buffer_size=bytes;CHECK(SWITCHAUDIO_OpenDevice(&d));CHECK(queue.size()==1);
 CHECK(d.spec.freq==48000&&d.spec.channels==2&&d.spec.format==SDL_AUDIO_S16LE);
 for(int frame=1;frame<=100;++frame){
 CHECK(queue.size()==1);int n=bytes;Uint8*mix=SWITCHAUDIO_GetDeviceBuf(&d,&n);CHECK(mix);std::memset(mix,frame,bytes);
 CHECK(SWITCHAUDIO_PlayDevice(&d,mix,bytes));CHECK(queue.size()==2);
 CHECK(!SWITCHAUDIO_PlayDevice(&d,mix,bytes)); // never overwrite a queued buffer
 if(frame==2)waitMode=1;if(frame==3)waitMode=2;
 CHECK(SWITCHAUDIO_WaitDevice(&d));CHECK(queue.size()==1);
 }
 CHECK(played.size()==100);for(size_t i=0;i<played.size();++i)for(auto b:played[i])CHECK(b==i);
 SWITCHAUDIO_CloseDevice(&d);CHECK(!d.hidden);CHECK(allocations.empty());
 }
 for(int fail=1;fail<=4;++fail){reset();failAlloc=fail;SDL_AudioDevice d;CHECK(!SWITCHAUDIO_OpenDevice(&d));CHECK(!d.hidden);CHECK(!initialized);CHECK(allocations.empty());}
 for(int stage=1;stage<=3;++stage){reset();failStage=stage;SDL_AudioDevice d;CHECK(!SWITCHAUDIO_OpenDevice(&d));CHECK(!d.hidden);CHECK(!initialized);CHECK(allocations.empty());}
 for(int mode: {3,4}){reset();SDL_AudioDevice d;CHECK(SWITCHAUDIO_OpenDevice(&d));int n=d.buffer_size;auto*b=SWITCHAUDIO_GetDeviceBuf(&d,&n);
 CHECK(!SWITCHAUDIO_PlayDevice(&d,b,n+4));CHECK(!SWITCHAUDIO_PlayDevice(&d,b,3));CHECK(SWITCHAUDIO_PlayDevice(&d,b,n));waitMode=mode;CHECK(!SWITCHAUDIO_WaitDevice(&d));SWITCHAUDIO_CloseDevice(&d);}
 reset();{SDL_AudioDevice d;CHECK(SWITCHAUDIO_OpenDevice(&d));int n=d.buffer_size;auto*b=SWITCHAUDIO_GetDeviceBuf(&d,&n);
 CHECK(SWITCHAUDIO_PlayDevice(&d,b,n));waitMode=5;CHECK(SWITCHAUDIO_WaitDevice(&d));CHECK(d.hidden->next==1);
 CHECK(SWITCHAUDIO_PlayDevice(&d,b,n));CHECK(SWITCHAUDIO_WaitDevice(&d));SWITCHAUDIO_CloseDevice(&d);}
 reset();{SDL_AudioDevice d;CHECK(SWITCHAUDIO_OpenDevice(&d));int n=d.buffer_size;auto*b=SWITCHAUDIO_GetDeviceBuf(&d,&n);
 failStage=2;CHECK(!SWITCHAUDIO_PlayDevice(&d,b,n));CHECK(queue.size()==1);CHECK(!d.hidden->queued[1]);SWITCHAUDIO_CloseDevice(&d);}
 reset();SDL_AudioDevice d;CHECK(SWITCHAUDIO_OpenDevice(&d));d.shutdown.value=1;CHECK(SWITCHAUDIO_WaitDevice(&d));SWITCHAUDIO_CloseDevice(&d);
 std::printf("PASS: %d checks, continuous PCM, ownership, timeout/spurious release, shutdown and failure cleanup\n",checks);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
