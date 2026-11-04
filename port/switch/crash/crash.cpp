// Controlled failures only. Raw Horizon exceptions remain with the system handler.
#include "crash/crash.hpp"
#include "crash/crash_capture.hpp"
#include "crash/platform.h"
#include <kazarov/log.hpp>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

namespace {
std::atomic_flag reporting = ATOMIC_FLAG_INIT;
std::uint64_t main_thread = 0;
bool initialized = false;
void Fatal(std::string_view subsystem, std::string_view message, bool unimplemented) {
    std::fwrite(subsystem.data(), 1, subsystem.size(), stderr);
    CrashReportAndExit(unimplemented ? CrashKind::Unimplemented : CrashKind::Fatal, message, 1);
}
void Terminate() {
    CrashReportAndExit(CrashKind::Terminate, "std::terminate called", 1);
}
}

void crash_detail::Capture::SetMessage(std::string_view text) {
    message_length = std::min(text.size(), sizeof(message) - 1);
    std::memcpy(message, text.data(), message_length);
    message[message_length] = 0;
}
std::uint64_t crash_detail::CurrentThreadId() { return ctCrashThreadId(); }
bool crash_detail::DialogsAllowed() { return false; }
void CrashInit(const CrashInitOptions &) {
    if (initialized) return;
    main_thread = ctCrashThreadId();
    crash_detail::WriterWarmUp();
    kz::SetFatalHook(Fatal);
    std::set_terminate(Terminate);
    initialized = true;
}
void CrashSetBundleRoot(const std::filesystem::path &root) {
    const auto utf8 = root.generic_u8string();
    crash_detail::WriterSetBundleRoot(std::string(reinterpret_cast<const char *>(utf8.data()), utf8.size()), "");
}
[[noreturn]] void CrashReportAndExit(CrashKind kind, std::string_view message, int status) {
    if (reporting.test_and_set()) std::_Exit(status ? status : 1);
    crash_detail::Capture capture;
    capture.kind = kind;
    capture.SetMessage(message);
    capture.thread_id = ctCrashThreadId();
    capture.main_thread = capture.thread_id == main_thread;
    capture.frame_count = ctCrashFrames(capture.frames, crash_detail::kMaxFrames);
    crash_detail::WriteBundle(capture, false);
    std::_Exit(status);
}
