#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>

// Presentation deadlines use the same steady clock as the game's field deadlines.
class PresentPacer {
public:
    using Clock = std::chrono::steady_clock;
    enum class Action {
        Tick,
        Wait,
        Display
    };

    struct Decision {
        Action            action;
        Clock::time_point deadline;
    };

    // A canonical image may be skipped while simulation continues. Unlike Next,
    // this query has no simulation deadline and never asks its caller to wait.
    [[nodiscard]] bool Ready(Clock::time_point now, double max_fps) const {
        return Next(now, Clock::time_point::max(), max_fps).action == Action::Display;
    }

    [[nodiscard]] Decision Next(Clock::time_point now, Clock::time_point next_tick, double max_fps) const {
        if (now >= next_tick) {
            return {.action = Action::Tick, .deadline = next_tick};
        }
        if (std::isfinite(max_fps) && max_fps > 0.0) {
            const auto period = std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(1.0 / std::max(1.0, max_fps)));
            const auto due = last_start_ > Clock::time_point::max() - period
                                 ? Clock::time_point::max()
                                 : last_start_ + period;
            if (now < due) {
                // Even a tick with no display yet must yield to the simulation deadline.
                return due >= next_tick ? Decision{.action = Action::Tick, .deadline = next_tick}
                                        : Decision{.action = Action::Wait, .deadline = due};
            }
        }
        return {.action = Action::Display, .deadline = now};
    }

    template <class Render, class Now>
    bool Present(Render render, Now now) {
        const auto start = now();
        if (!render()) {
            return false;
        }
        // Keep no debt for missed display slots: the next deadline is one period
        // after this presentation's actual start, regardless of rendering cost.
        last_start_ = start;
        return true;
    }

private:
    Clock::time_point last_start_;
};

// Canonical images arrive at the game's cadence, often with a little jitter.
// Assign them to nearest display slots instead of enforcing a minimum interval
// between actual starts (which can discard every other 30 Hz image). The cap is
// a nominal slot rate: adjacent actual starts may be less than one period apart.
// Actual starts are also separated by at least half a period, bounding jitter
// compensation to two starts per period. Only successful presentations consume
// a slot. Missing a complete slot resets the phase, so a long stall cannot
// accumulate a burst of overdue presentations.
class CanonicalPresentPacer {
public:
    using Clock = PresentPacer::Clock;

    void Reset() { initialized_ = false; }
    [[nodiscard]] bool Active() const { return initialized_; }

    template <class Render, class Now>
    bool Present(Render render, Now now, double max_fps) {
        const auto start = now();
        const bool capped = std::isfinite(max_fps) && max_fps > 0.0;
        const auto period = capped ? std::chrono::duration_cast<Clock::duration>(
                                         std::chrono::duration<double>(1.0 / std::max(1.0, max_fps)))
                                   : Clock::duration::zero();
        const bool same_phase = initialized_ && period > Clock::duration::zero() && period == period_;
        // Half-period boundaries assign an arrival to its nearest target slot;
        // this is a cadence policy, not a floating-point comparison tolerance.
        if (same_phase && (start <= last_start_ ||
            start < std::max(next_slot_ - period / 2, Add(last_start_, period / 2)))) {
            return false;
        }
        if (!render()) {
            return false;
        }
        if (period > Clock::duration::zero()) {
            const auto base = same_phase && start < Add(next_slot_, period / 2) ? next_slot_ : start;
            next_slot_ = Add(base, period);
            last_start_ = start;
            period_ = period;
            initialized_ = true;
        } else {
            initialized_ = false;
        }
        return true;
    }

private:
    static Clock::time_point Add(Clock::time_point base, Clock::duration delta) {
        return base > Clock::time_point::max() - delta ? Clock::time_point::max() : base + delta;
    }
    Clock::time_point next_slot_{};
    Clock::time_point last_start_{};
    Clock::duration period_{};
    bool initialized_ = false;
};

// A capped display at or below the game's native image rate can show the
// completed canonical image. Only a faster display needs another scene pass.
inline bool PresentNeedsIntermediateFrames(double max_fps, double field_rate, int frame_fields) {
    if (!std::isfinite(max_fps) || max_fps <= 0.0 ||
        !std::isfinite(field_rate) || field_rate <= 0.0 || frame_fields <= 0) {
        return true;
    }
    return max_fps > field_rate / frame_fields;
}

inline float PresentFrameAlpha(double elapsed_fields, double fractional_field, int frame_fields) {
    const double alpha = frame_fields > 0 ? (elapsed_fields + fractional_field) / frame_fields
                                          : fractional_field;
    return static_cast<float>(std::clamp(alpha, 0.0, std::nextafter(1.0, 0.0)));
}
