#include "crash/crash.hpp"
#include "crash/crash_capture.hpp"
#include "crash/platform.h"
#include "log/log.hpp"
#include <fstream>
#include <iterator>
#include <string>

extern "C" int ctSwitchServicesChecks() {
    const std::string root = "sdmc:/switch/ChronicleTwo-PC-Switch/services-probe/saves";
    CrashInit({.headless=true});
    CrashSetBundleRoot(root);
    CrashContextSet(CrashKey::Loop, "services-probe");
    CrashContextSetInt(CrashKey::Frame, 77);
    // Context slots are bounded to 64 bytes; store the relative asset name.
    CrashContextSet(CrashKey::LastAsset, "services-probe/asset.bin");
    crash_detail::Capture capture;
    capture.kind = CrashKind::Fatal;
    capture.SetMessage(root + "/synthetic report; process stays running");
    capture.thread_id = ctCrashThreadId(); capture.main_thread = true;
    capture.frame_count = ctCrashFrames(capture.frames, crash_detail::kMaxFrames);
    crash_detail::WriteBundle(capture, false);
    std::ifstream input(root + "/crashes/switch-last-report.txt", std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    int failures = 0;
    if (text.find("Dark Chronicle crash report") == std::string::npos ||
        text.find("synthetic report; process stays running") == std::string::npos) failures |= 1;
    if (text.find("loop: services-probe") == std::string::npos || text.find("frame: 77") == std::string::npos ||
        text.find("last asset: services-probe/asset.bin") == std::string::npos ||
        text.find("<save>/synthetic report") == std::string::npos || text.find(root) != std::string::npos) failures |= 2;
    if (!capture.frame_count || !capture.thread_id || !ctCrashModuleBase() ||
        text.find("module+0x") == std::string::npos || text.find("frame 0:") == std::string::npos) failures |= 4;
    CT_LOG_FILE_INFO("services report readback: {}; context/redaction: {}; raw frames: {}",
        !(failures & 1), !(failures & 2), !(failures & 4));
    LogFlush();
    return failures;
}
