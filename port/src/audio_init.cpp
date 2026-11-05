#include "audio_init.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#ifdef __SWITCH__
#include <atomic>
#include <chrono>
#endif

#include "audio/mixer.hpp"
#include "log/log.hpp"
#include "platform/audio.hpp"
#include "platform/clock.hpp"
#include "platform/settings.hpp"

namespace {

bool         g_tick_audio = false;
std::int64_t g_last_tick = 0;
double       g_fraction = 0.0;
double       g_span_fraction = 0.0;
double       g_tick_rate = 0.0;
std::int64_t g_span_start = 0;
std::int64_t g_span_frames = 0;
#ifdef __SWITCH__
std::atomic<std::uint64_t> g_audio_calls{0},g_audio_us{0},g_audio_peak_us{0},g_audio_over_budget{0};
void ReportAudioTiming() {
    using Clock=std::chrono::steady_clock;
    static auto previous=Clock::now();
    const auto now=Clock::now();
    if(now-previous<std::chrono::seconds(5))return;
    previous=now;
    const auto calls=g_audio_calls.exchange(0,std::memory_order_relaxed);
    const auto elapsed=g_audio_us.exchange(0,std::memory_order_relaxed);
    const auto peak=g_audio_peak_us.exchange(0,std::memory_order_relaxed);
    const auto late=g_audio_over_budget.exchange(0,std::memory_order_relaxed);
    if(calls)CT_LOG_INFO(Audio,"mix callbacks {}, mean {} us, max {} us, over budget {}",calls,elapsed/calls,peak,late);
}
#endif

void RenderTicks() {
    const auto   tick = ClockTickCount();
    const double rate = ClockTickRate();
    if (rate != g_tick_rate || tick < g_last_tick) {
        g_span_start = tick < g_last_tick ? tick : g_last_tick;
        g_span_fraction = g_fraction;
        g_span_frames = 0;
        g_tick_rate = rate;
    }
    g_last_tick = tick;
    auto &mixer = audio::DefaultMixer();
    // Compute the cumulative budget before truncation, so fractional samples cannot lose a whole
    // sample through repeated addition. Keep the remainder when the tick rate changes.
    const double budget = g_span_fraction + static_cast<double>(tick - g_span_start) * mixer.Rate() / rate;
    const auto   total = static_cast<std::int64_t>(budget);
    auto         frames = total - g_span_frames;
    g_span_frames = total;
    g_fraction = budget - static_cast<double>(total);
    std::array<float, 2048> samples{};
    while (frames > 0) {
        const int count = static_cast<int>(std::min<std::int64_t>(frames, samples.size() / 2));
        mixer.Render(samples.data(), count);
        frames -= count;
    }
}

void RenderMixer(void *user, float *out, int frames) {
    // The device thread's first callback readies its log queue, so a later line from it never allocates.
    thread_local const bool log_ready = (LogThreadReady(), true);
    static_cast<void>(log_ready);
    // Mixer::Render takes the mixer's own lock, and every game-thread setter takes it too, so the
    // device thread needs no lock of its own here.
#ifdef __SWITCH__
    const auto started=std::chrono::steady_clock::now();
#endif
    auto *mixer=static_cast<audio::Mixer *>(user);
    mixer->Render(out, frames);
#ifdef __SWITCH__
    const auto micros=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-started).count());
    g_audio_calls.fetch_add(1,std::memory_order_relaxed);
    g_audio_us.fetch_add(micros,std::memory_order_relaxed);
    auto peak=g_audio_peak_us.load(std::memory_order_relaxed);
    while(peak<micros && !g_audio_peak_us.compare_exchange_weak(peak,micros,std::memory_order_relaxed)){}
    if(micros>static_cast<std::uint64_t>(frames)*1000000/mixer->Rate())
        g_audio_over_budget.fetch_add(1,std::memory_order_relaxed);
#endif
}

// AudioOutputStart returns false for "off" and for a failed device alike, so "off" is recognised
// here before the platform layer is asked. The WAV capture outranks "off" there and must here too.
bool OutputDisabled() {
    if (const char *wav = SettingsEnv("AUDIO_WAV"); wav != nullptr && *wav != '\0') {
        return false;
    }
    const char *setting = SettingsEnv("AUDIO");
    return setting != nullptr && std::strcmp(setting, "off") == 0;
}

} // namespace

bool AudioUsesTickClock() {
    return g_tick_audio;
}

bool AudioInit(bool headless) {
    if (headless) {
        // Stream levels and completion are game inputs (voice animation and script waits).
        // Device callbacks run in real time, while headless VSyncs run as fast as possible.
        // A requested WAV capture already renders from VSyncs in the platform layer.
        const char *wav = SettingsEnv("AUDIO_WAV");
        if (wav == nullptr || *wav == '\0') {
            g_tick_audio = true;
            g_last_tick = ClockTickCount();
            g_fraction = 0.0;
            g_span_fraction = 0.0;
            g_tick_rate = ClockTickRate();
            g_span_start = g_last_tick;
            g_span_frames = 0;
            ClockAddPumpHook(RenderTicks);
            return true;
        }
    }
    if (OutputDisabled()) {
        return true;
    }
    audio::Mixer &mixer = audio::DefaultMixer();
    const bool started=AudioOutputStart(mixer.Rate(), RenderMixer, &mixer);
#ifdef __SWITCH__
    if(started)ClockAddPumpHook(ReportAudioTiming);
#endif
    return started;
}

void AudioShutdown() {
#ifdef __SWITCH__
    ClockRemovePumpHook(ReportAudioTiming);
#endif
    ClockRemovePumpHook(RenderTicks);
    g_tick_audio = false;
    g_fraction = 0.0;
    AudioOutputStop();
}
