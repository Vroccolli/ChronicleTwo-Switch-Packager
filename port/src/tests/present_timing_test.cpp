#include <gtest/gtest.h>

#include "present_timing.hpp"

namespace {
using Clock = PresentPacer::Clock;
using Action = PresentPacer::Action;
using namespace std::chrono_literals;
const auto origin = Clock::time_point{} + 1s;
} // namespace

TEST(PresentTiming, RenderTimeIsPartOfTheCapPeriod) {
    for (double rate : {60.0, 120.0, 240.0}) {
        PresentPacer pacer;
        auto         now = origin;
        const auto   period = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1 / rate));
        ASSERT_TRUE(pacer.Present([&] { now += 2ms; return true; }, [&] { return now; }));
        const auto decision = pacer.Next(now, origin + 40ms, rate);
        EXPECT_EQ(decision.action, Action::Wait);
        EXPECT_EQ(decision.deadline, origin + period);
        EXPECT_EQ(pacer.Next(origin + period, origin + 40ms, rate).action, Action::Display);
    }
}

TEST(PresentTiming, SlowRenderAndStallSkipExpiredSlotsWithoutCatchUpBursts) {
    PresentPacer pacer;
    auto         now = origin;
    ASSERT_TRUE(pacer.Present([&] { now += 13ms; return true; }, [&] { return now; }));
    EXPECT_EQ(pacer.Next(now, origin + 40ms, 240).action, Action::Display);
    now += 200ms;
    const auto restarted = now;
    ASSERT_TRUE(pacer.Present([&] { now += 1ms; return true; }, [&] { return now; }));
    auto decision = pacer.Next(now, now + 40ms, 240);
    EXPECT_EQ(decision.action, Action::Wait);
    const auto period = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / 240));
    EXPECT_EQ(decision.deadline, restarted + period);
}

TEST(PresentTiming, DueGameFieldWinsEvenWithoutADisplay) {
    PresentPacer pacer;
    auto         now = origin;
    ASSERT_TRUE(pacer.Present([] { return true; }, [&] { return now; }));
    EXPECT_EQ(pacer.Next(now, now + 2ms, 240).action, Action::Tick);
    EXPECT_EQ(pacer.Next(now + 20ms, now + 20ms, 0).action, Action::Tick);
    EXPECT_EQ(pacer.Next(now + 21ms, now + 20ms, 240).action, Action::Tick);
    EXPECT_EQ(pacer.Next(now + 20ms, now + 40ms, 0).action, Action::Display);
}

TEST(PresentTiming, UnlimitedAndFailedPresentDoNotAccumulateWaits) {
    PresentPacer pacer;
    auto         now = origin;
    ASSERT_TRUE(pacer.Present([&] { now += 2ms; return true; }, [&] { return now; }));
    EXPECT_EQ(pacer.Next(now, origin + 20ms, 0).action, Action::Display);
    now = origin + 10ms;
    EXPECT_FALSE(pacer.Present([] { return false; }, [&] { return now; }));
    EXPECT_EQ(pacer.Next(now, origin + 20ms, 240).action, Action::Display);
}

TEST(PresentTiming, AlphaSpansTheWholeOneOrTwoFieldGameFrame) {
    EXPECT_FLOAT_EQ(PresentFrameAlpha(0, .5, 1), .5f);
    EXPECT_FLOAT_EQ(PresentFrameAlpha(0, .5, 2), .25f);
    EXPECT_FLOAT_EQ(PresentFrameAlpha(1, .5, 2), .75f);
    EXPECT_FLOAT_EQ(PresentFrameAlpha(0, 0, 2), 0);
    EXPECT_LE(PresentFrameAlpha(3, .5, 2), 1);
}

TEST(PresentTiming, TinyCapsAreOneFpsAndZeroIsUnlimited) {
    PresentPacer pacer;
    ASSERT_TRUE(pacer.Present([] { return true; }, [] { return origin; }));
    for (double rate : {1e-12, 1e-300, 0.5, 1.0}) {
        auto decision = pacer.Next(origin + 1ms, origin + 2s, rate);
        EXPECT_EQ(decision.action, Action::Wait);
        EXPECT_EQ(decision.deadline, origin + 1s);
    }
    EXPECT_EQ(pacer.Next(origin + 1ms, origin + 2s, 0).action, Action::Display);
    const auto end = Clock::time_point::max() - 2ms;
    ASSERT_TRUE(pacer.Present([] { return true; }, [&] { return end; }));
    EXPECT_EQ(pacer.Next(end + 1ms, Clock::time_point::max(), 1e-12).action, Action::Tick);
}

TEST(PresentTiming, CanonicalCapKeepsSixtySimulationFieldsAndThirtyDisplays) {
    PresentPacer pacer;
    int fields = 0;
    int displays = 0;
    for (int field = 0; field < 600; ++field) {
        // Absolute field deadlines avoid accumulating rounded clock durations.
        const auto now = origin + std::chrono::nanoseconds(1000000000LL * field / 60);
        ++fields;
        if (pacer.Ready(now, 30)) {
            ASSERT_TRUE(pacer.Present([] { return true; }, [&] { return now; }));
            ++displays;
        }
    }
    EXPECT_EQ(fields, 600);
    EXPECT_EQ(displays, 300);
}

TEST(PresentTiming, CanonicalCapSharesInterpolatedDeadlineAndSkipsStallDebt) {
    PresentPacer pacer;
    auto now = origin;
    ASSERT_TRUE(pacer.Ready(now, 30));
    ASSERT_TRUE(pacer.Present([] { return true; }, [&] { return now; }));
    now += 16ms;
    EXPECT_FALSE(pacer.Ready(now, 30));
    EXPECT_TRUE(pacer.Ready(now, 0));
    EXPECT_EQ(pacer.Next(now, now + 1ms, 30).action, Action::Tick);
    now += 1s;
    ASSERT_TRUE(pacer.Ready(now, 30));
    ASSERT_TRUE(pacer.Present([] { return true; }, [&] { return now; }));
    EXPECT_FALSE(pacer.Ready(now, 30));
    EXPECT_EQ(pacer.Next(now, now + 1s, 30).action, Action::Wait);
}

TEST(PresentTiming, ThirtyFpsReusesCanonicalAcrossOneAndTwoFieldCameraTransitions) {
    for (int fields : {2, 1, 2, 1, 2}) {
        EXPECT_FALSE(PresentNeedsIntermediateFrames(30, 60, fields));
        EXPECT_FALSE(PresentNeedsIntermediateFrames(24, 60, fields));
    }
    // A 20 Hz game image stream still needs intermediate images for 30 FPS.
    EXPECT_TRUE(PresentNeedsIntermediateFrames(30, 60, 3));
    EXPECT_TRUE(PresentNeedsIntermediateFrames(60, 60, 2));
    EXPECT_FALSE(PresentNeedsIntermediateFrames(60, 60, 1));
}

TEST(PresentTiming, UnlimitedAndUnknownImageCadenceRetainIntermediateFrames) {
    EXPECT_TRUE(PresentNeedsIntermediateFrames(0, 60, 2));
    EXPECT_TRUE(PresentNeedsIntermediateFrames(30, 0, 2));
    EXPECT_TRUE(PresentNeedsIntermediateFrames(30, 60, 0));
    EXPECT_TRUE(PresentNeedsIntermediateFrames(30, 60, -1));
    EXPECT_TRUE(PresentNeedsIntermediateFrames(INFINITY, 60, 2));
    EXPECT_TRUE(PresentNeedsIntermediateFrames(30, INFINITY, 2));
}

TEST(PresentTiming, CanonicalSlotsKeepJitteredThirtyHzImagesAtThirtyHz) {
    for (int sign : {-1, 1}) {
        CanonicalPresentPacer pacer;
        int displays = 0;
        for (int image = 0; image < 600; ++image) {
            // Alternating near-period intervals reproduced the former half-rate lock.
            const auto now = origin + std::chrono::nanoseconds(1000000000LL * image / 30) +
                             std::chrono::microseconds(sign * (image % 2 ? 900 : -900));
            if (pacer.Present([] { return true; }, [&] { return now; }, 30)) ++displays;
            EXPECT_FALSE(pacer.Present([] { return true; }, [&] { return now; }, 30));
        }
        EXPECT_EQ(displays, 600);
    }
}

TEST(PresentTiming, CanonicalSlotsSelectThirtyFromJitteredSixtyHzImages) {
    CanonicalPresentPacer pacer;
    int displays = 0;
    for (int image = 0; image < 1200; ++image) {
        const auto now = origin + std::chrono::nanoseconds(1000000000LL * image / 60) +
                         std::chrono::microseconds((image % 7 - 3) * 300);
        if (pacer.Present([] { return true; }, [&] { return now; }, 30)) ++displays;
    }
    EXPECT_GE(displays, 599);
    EXPECT_LE(displays, 601);
}

TEST(PresentTiming, CanonicalSlotsSurviveOneAndTwoFieldCameraCadences) {
    CanonicalPresentPacer pacer;
    int displays = 0;
    int expected = 0;
    int field = 0;
    for (int fields : {2, 1, 2, 1, 2}) {
        for (int image = 0; image < 120; ++image) {
            const auto now = origin + std::chrono::nanoseconds(1000000000LL * field / 60) +
                             std::chrono::microseconds((image % 5 - 2) * 250);
            if (pacer.Present([] { return true; }, [&] { return now; }, 30)) ++displays;
            field += fields;
        }
        expected += fields == 2 ? 120 : 60;
    }
    EXPECT_GE(displays, expected - 2);
    EXPECT_LE(displays, expected + 2);
}

TEST(PresentTiming, CanonicalFailureDoesNotConsumeSlotAndStallHasNoDebt) {
    CanonicalPresentPacer pacer;
    auto now = origin;
    EXPECT_FALSE(pacer.Present([] { return false; }, [&] { return now; }, 30));
    EXPECT_TRUE(pacer.Present([] { return true; }, [&] { return now; }, 30));
    EXPECT_FALSE(pacer.Present([] { return true; }, [&] { return now; }, 30));
    now += 5s;
    EXPECT_TRUE(pacer.Present([] { return true; }, [&] { return now; }, 30));
    for (int ms = 0; ms < 16; ++ms) {
        const auto retry = now + std::chrono::milliseconds(ms);
        EXPECT_FALSE(pacer.Present([] { return true; }, [&] { return retry; }, 30));
    }
    now += 33ms;
    EXPECT_TRUE(pacer.Present([] { return true; }, [&] { return now; }, 30));
}

TEST(PresentTiming, CanonicalUnlimitedAndCapChangesStartAFreshPhase) {
    CanonicalPresentPacer pacer;
    auto now = origin;
    EXPECT_TRUE(pacer.Present([] { return true; }, [&] { return now; }, 30));
    EXPECT_TRUE(pacer.Present([] { return true; }, [&] { return now; }, 0));
    EXPECT_TRUE(pacer.Present([] { return true; }, [&] { return now; }, 0));
    EXPECT_TRUE(pacer.Present([] { return true; }, [&] { return now; }, 30));
    EXPECT_TRUE(pacer.Present([] { return true; }, [&] { return now; }, 60));
    EXPECT_FALSE(pacer.Present([] { return true; }, [&] { return now; }, 60));
}

TEST(PresentTiming, CanonicalSlotsPreserveOrdinaryPacingAcrossModeTransitions) {
    PresentPacer ordinary;
    CanonicalPresentPacer canonical;
    auto now = origin;
    auto actual = [&] { return ordinary.Present([] { return true; }, [&] { return now; }); };
    ASSERT_TRUE(canonical.Present(actual, [&] { return now; }, 30));
    // Entering interpolation uses the actual canonical start, not an old one.
    canonical.Reset();
    EXPECT_FALSE(canonical.Active());
    EXPECT_EQ(ordinary.Next(now + 1ms, now + 1s, 30).action, Action::Wait);
    now += 34ms;
    ASSERT_TRUE(actual());
    // Re-entering the native policy must pass the ordinary Ready guard first.
    EXPECT_FALSE(ordinary.Ready(now + 1ms, 30));
    now += 34ms;
    ASSERT_TRUE(ordinary.Ready(now, 30));
    ASSERT_TRUE(canonical.Present(actual, [&] { return now; }, 30));
    now += 34ms;
    EXPECT_FALSE(canonical.Present([&] { return ordinary.Present([] { return false; }, [&] { return now; }); },
                                  [&] { return now; }, 30));
    EXPECT_TRUE(ordinary.Ready(now, 30));
    EXPECT_TRUE(canonical.Present(actual, [&] { return now; }, 30));
}

TEST(PresentTiming, CanonicalSlotsSaturateClockEdgesWithoutOverflowOrDuplicates) {
    for (auto start : {Clock::time_point::min(), Clock::time_point::max() - 2ms}) {
        CanonicalPresentPacer pacer;
        auto now = start;
        ASSERT_TRUE(pacer.Present([] { return true; }, [&] { return now; }, 1e-300));
        EXPECT_FALSE(pacer.Present([] { return true; }, [&] { return now; }, 1e-300));
        now += 1ms;
        EXPECT_FALSE(pacer.Present([] { return true; }, [&] { return now; }, 1e-300));
    }
}
