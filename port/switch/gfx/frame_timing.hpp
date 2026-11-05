#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>

namespace ct::frame_timing {
using Clock = std::chrono::steady_clock;
enum class Phase { EndFrame, Canonical, Readback, Display, CanonicalPresent,
                   Acquire, Fence, QueuePresent, Submit, Interpolation, Matching, Count };
struct Metric {
    std::atomic<unsigned long long> calls{0}, total{0}, peak{0};
};
inline std::array<Metric, static_cast<unsigned>(Phase::Count)> metrics;

// No logging or allocation in a measured phase. This remains safe if a later
// hardware test enables the render worker.
class Scope {
public:
    explicit Scope(Phase phase) : metric(metrics[static_cast<unsigned>(phase)]), start(Clock::now()) {}
    ~Scope() {
        const auto us = static_cast<unsigned long long>(
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count());
        metric.calls.fetch_add(1, std::memory_order_relaxed);
        metric.total.fetch_add(us, std::memory_order_relaxed);
        auto peak = metric.peak.load(std::memory_order_relaxed);
        while (peak < us && !metric.peak.compare_exchange_weak(peak, us, std::memory_order_relaxed)) {}
    }
private:
    Metric &metric;
    Clock::time_point start;
};

// Called only by the game thread. Nested phase times overlap; they must not
// be added together as an exclusive CPU/GPU breakdown.
inline void Report(int frame_fields, bool interpolation, double cap) {
    static auto previous = Clock::now();
    const auto now = Clock::now();
    if (now - previous < std::chrono::seconds(5)) return;
    previous = now;
    std::fprintf(stderr, "frame timing: fields %d, interpolation %d, cap %.1f\n",
                 frame_fields, interpolation, cap);
    constexpr const char *names[] = {"end-frame", "canonical", "readback", "display", "canonical-present",
                                    "acquire", "fence", "queue-present", "submit", "interpolation", "matching"};
    for (unsigned i = 0; i < metrics.size(); ++i) {
        auto &m = metrics[i];
        const auto calls = m.calls.exchange(0, std::memory_order_relaxed);
        const auto total = m.total.exchange(0, std::memory_order_relaxed);
        const auto peak = m.peak.exchange(0, std::memory_order_relaxed);
        if (calls) std::fprintf(stderr, "frame phase %s: %llu calls, mean %llu us, max %llu us\n",
                                names[i], calls, total / calls, peak);
    }
}
} // namespace ct::frame_timing
