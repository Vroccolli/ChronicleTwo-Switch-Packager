#include "crash/crash_capture.hpp"
#include "crash/crash_detail.hpp"
#include "crash/platform.h"
#include "log/log.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
extern "C" const char *ctSwitchGameBuildId(void) __attribute__((weak));

namespace {
std::string directory, report;
crash_detail::RedactRoots roots;
std::FILE *active = nullptr;
}
namespace crash_detail {
void WriterWarmUp() {}
void WriterSetBundleRoot(const std::string &save, const std::string &data) {
    directory = save + "/crashes";
    report = directory + "/switch-last-report.txt";
    roots = {save, data, ""};
    std::error_code error;
    std::filesystem::create_directories(directory, error);
}
bool WriterHasBundle() { return !report.empty(); }
std::string_view WriterBundleDir() { return directory; }
std::string_view WriterReportPath() { return report; }
std::string_view PlatformName() { return "arm64-switch"; }
std::string_view BuildId() {
    return ctSwitchGameBuildId ? ctSwitchGameBuildId() : "switch-services-0.4.1";
}
void AppendRaw(std::string_view text) {
    std::fwrite(text.data(), 1, text.size(), stderr);
    if (active) std::fwrite(text.data(), 1, text.size(), active);
}
void WriteBundle(Capture &capture, bool) {
    // This path runs in ordinary thread context, never in a raw fault/signal handler.
    if (!report.empty()) active = std::fopen(report.c_str(), "wb");
    if (!active) std::fputs("Switch crash report file unavailable; writing to stderr\n", stderr);
    char buffer[4096];
    Text text(buffer, sizeof(buffer));
    FormatHeader(text, {.kind=CrashKindName(capture.kind), .session=LogSessionId(),
        .build_id=BuildId(), .build_type="bring-up", .platform=PlatformName()});
    text.Put("capture: controlled failure; raw CPU faults use the system handler\n");
    text.Put("thread: ").Hex(capture.thread_id).Put(capture.main_thread ? " main\n" : " worker\n");
    text.Put("message: ").Put(RedactPath({capture.message, capture.message_length}, roots)).Put('\n');
    text.Put("module base: ").Pointer(ctCrashModuleBase()).Put('\n');
    AppendRaw(text.View());
    for (std::size_t i = 0; i < static_cast<std::size_t>(CrashKey::Count); ++i) {
        std::array<char, kSlotSize> value{};
        auto key = static_cast<CrashKey>(i);
        auto count = ContextRead(key, value);
        text.Clear(); text.Put(CrashKeyName(key)).Put(": ").Put(RedactPath({value.data(), count}, roots)).Put('\n');
        AppendRaw(text.View());
    }
    for (std::size_t i = 0; i < capture.frame_count; ++i) {
        text.Clear(); text.Put("frame ").Dec(i).Put(": ").Pointer(capture.frames[i]);
        text.Put(" ").Put(SymbolTableName(capture.frames[i])).Put('\n');
        AppendRaw(text.View());
    }
    text.Clear(); text.Put("log: ").Put(RedactPath(LogFilePath().string(), roots)).Put('\n');
    AppendRaw(text.View());
    if (active) {
        if (std::fclose(active)) std::fputs("Switch crash report close failed\n", stderr);
        active = nullptr;
    }
    // The synchronous backend refuses re-entry and bounds lock acquisition.
    LogFlushForCrash(std::chrono::milliseconds(100));
    std::fflush(stderr);
}
}
