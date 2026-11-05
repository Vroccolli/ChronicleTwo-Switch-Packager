#pragma once
struct SDL_PrivateAudioData;
struct SDL_AudioDevice {
 struct { int freq,channels,format; } spec;
 int buffer_size=4096; int silence_value=0; SDL_AtomicInt shutdown={0};
 SDL_PrivateAudioData *hidden=nullptr;
};
inline void SDL_UpdatedAudioDeviceFormat(SDL_AudioDevice*){}
struct SDL_AudioDriverImpl {
 bool (*OpenDevice)(SDL_AudioDevice*);
 Uint8* (*GetDeviceBuf)(SDL_AudioDevice*,int*);
 bool (*PlayDevice)(SDL_AudioDevice*,const Uint8*,int);
 bool (*WaitDevice)(SDL_AudioDevice*);
 void (*CloseDevice)(SDL_AudioDevice*);
 bool OnlyHasDefaultPlaybackDevice,HasRecordingSupport;
};
struct AudioBootStrap { const char*name; const char*desc; bool(*init)(SDL_AudioDriverImpl*); bool a,b; };
