/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
// ChronicleTwo adaptation: keep an AUDOUT buffer queued while SDL mixes.
#include "SDL_internal.h"
#ifdef SDL_AUDIO_DRIVER_SWITCH
#include "audio/SDL_sysaudio.h"
#include <malloc.h>
#include <switch.h>

#define NUM_AUDIO_BUFFERS 2
#define SWITCH_AUDIO_ALIGN 0x1000
#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((u64)(a) - 1))
struct SDL_PrivateAudioData {
    Uint8 *mixbuf;
    AudioOutBuffer buffers[NUM_AUDIO_BUFFERS];
    bool queued[NUM_AUDIO_BUFFERS];
    int next;
};

static void SWITCHAUDIO_FreeDevice(SDL_AudioDevice *device)
{
    for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
        free(device->hidden->buffers[i].buffer);
    }
    SDL_free(device->hidden->mixbuf);
    SDL_free(device->hidden);
    device->hidden = NULL;
}

static bool SWITCHAUDIO_OpenDevice(SDL_AudioDevice *device)
{
    bool initialized = false;
    device->hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) return false;
    device->spec.freq = 48000;
    device->spec.channels = 2;
    device->spec.format = SDL_AUDIO_S16LE;
    SDL_UpdatedAudioDeviceFormat(device);
    if (R_FAILED(audoutInitialize())) {
        SDL_SetError("audoutInitialize() failed");
        goto failed;
    }
    initialized = true;
    for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
        AudioOutBuffer *b = &device->hidden->buffers[i];
        b->buffer_size = ALIGN_UP((u64)device->buffer_size, SWITCH_AUDIO_ALIGN);
        b->buffer = memalign(SWITCH_AUDIO_ALIGN, b->buffer_size);
        if (!b->buffer) {
            SDL_SetError("Out of memory allocating AUDOUT buffer");
            goto failed;
        }
        SDL_memset(b->buffer, 0, b->buffer_size);
        b->data_size = device->buffer_size;
    }
    device->hidden->mixbuf = (Uint8 *)SDL_malloc(device->buffer_size);
    if (!device->hidden->mixbuf) goto failed;
    SDL_memset(device->hidden->mixbuf, device->silence_value, device->buffer_size);

    // SDL calls PlayDevice then WaitDevice. Prime one buffer before starting,
    // leaving the other free for the first mix (as in devkitPro SDL's
    // switch-sdl-2.28-audout backend). Waiting then releases one slot
    // while the other remains queued for continuous playback.
    if (R_FAILED(audoutAppendAudioOutBuffer(&device->hidden->buffers[0]))) {
        SDL_SetError("audoutAppendAudioOutBuffer() failed during priming");
        goto failed;
    }
    device->hidden->queued[0] = true;
    device->hidden->next = 1;
    if (R_FAILED(audoutStartAudioOut())) {
        SDL_SetError("audoutStartAudioOut() failed");
        goto failed;
    }
    return true;
failed:
    // AUDOUT must release queued storage before it is freed, including a
    // primed buffer when starting the output fails.
    if (initialized) {
        audoutStopAudioOut();
        audoutExit();
    }
    SWITCHAUDIO_FreeDevice(device);
    return false;
}

static Uint8 *SWITCHAUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    (void)buffer_size;
    int next = device->hidden->next;
    if (next < 0 || next >= NUM_AUDIO_BUFFERS || device->hidden->queued[next]) {
        SDL_SetError("No released AUDOUT buffer available");
        return NULL;
    }
    return device->hidden->mixbuf;
}

static bool SWITCHAUDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buflen)
{
    int next = device->hidden->next;
    if (next < 0 || next >= NUM_AUDIO_BUFFERS || device->hidden->queued[next] ||
        buflen <= 0 || buflen > device->buffer_size || (buflen % 4) != 0) {
        return SDL_SetError("Invalid AUDOUT buffer submission");
    }
    AudioOutBuffer *b = &device->hidden->buffers[next];
    SDL_memcpy(b->buffer, buffer, buflen);
    b->data_size = (u64)buflen;
    if (R_FAILED(audoutAppendAudioOutBuffer(b))) {
        return SDL_SetError("audoutAppendAudioOutBuffer() failed");
    }
    device->hidden->queued[next] = true;
    device->hidden->next = -1;
    return true;
}

static bool SWITCHAUDIO_WaitDevice(SDL_AudioDevice *device)
{
    // A finite wait lets SDL close cleanly even if AUDOUT stops delivering
    // releases. Successful waits with no released tag are possible; retry.
    while (!SDL_GetAtomicInt(&device->shutdown)) {
        AudioOutBuffer *released = NULL;
        u32 count = 0;
        Result rc = audoutWaitPlayFinish(&released, &count, 100000000);
        if (rc == KERNELRESULT(TimedOut)) continue;
        if (R_FAILED(rc)) return SDL_SetError("audoutWaitPlayFinish() failed");
        if (!count) continue;
        if (count == 1) {
            for (int i = 0; i < NUM_AUDIO_BUFFERS; ++i) {
                if (released == &device->hidden->buffers[i] && device->hidden->queued[i]) {
                    device->hidden->queued[i] = false;
                    device->hidden->next = i;
                    return true;
                }
            }
        }
        return SDL_SetError("AUDOUT returned an invalid released buffer");
    }
    return true;
}

static void SWITCHAUDIO_CloseDevice(SDL_AudioDevice *device)
{
    if (!device->hidden) return;
    audoutStopAudioOut();
    audoutExit();
    SWITCHAUDIO_FreeDevice(device);
}

static bool SWITCHAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    impl->OpenDevice = SWITCHAUDIO_OpenDevice;
    impl->GetDeviceBuf = SWITCHAUDIO_GetDeviceBuf;
    impl->PlayDevice = SWITCHAUDIO_PlayDevice;
    impl->WaitDevice = SWITCHAUDIO_WaitDevice;
    impl->CloseDevice = SWITCHAUDIO_CloseDevice;
    impl->OnlyHasDefaultPlaybackDevice = true;
    impl->HasRecordingSupport = false;
    return true;
}
AudioBootStrap SWITCHAUDIO_bootstrap = {
    "switch", "SDL Nintendo Switch (libnx AUDOUT) audio driver",
    SWITCHAUDIO_Init, false, false
};
#endif
