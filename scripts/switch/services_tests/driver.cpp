#include "crash/crash.hpp"
#include "crash/crash_capture.hpp"
#include "crash/platform.h"
#include "log/log.hpp"
#include <kazarov/log.hpp>
#include <exception>
#include <string>
#include <thread>
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    const std::string mode = argv[1];
    CrashInit({.headless=true});
    if (mode != "no-root") CrashSetBundleRoot(argv[2]);
    CrashContextSet(CrashKey::Loop, "services-test");
    CrashContextSetInt(CrashKey::Frame, 77);
    CrashContextSet(CrashKey::LastAsset, "services-test/asset.bin");
    if (mode == "terminate") std::terminate();
    if (mode == "fatal-hook") kz::Abort("test", "fatal hook marker");
    if (mode == "worker") {
        std::thread worker([] { CrashReportAndExit(CrashKind::Assert, "worker marker", 23); });
        worker.join();
    }
    if (mode == "write") {
        crash_detail::Capture capture;
        capture.kind = CrashKind::Fatal;
        capture.SetMessage(std::string(argv[2]) + "/report marker");
        capture.thread_id = ctCrashThreadId(); capture.main_thread = true;
        capture.frame_count = ctCrashFrames(capture.frames, crash_detail::kMaxFrames);
        crash_detail::WriteBundle(capture, false);
        return capture.frame_count ? 0 : 3;
    }
    CrashReportAndExit(CrashKind::Fatal, "controlled marker", 17);
}
