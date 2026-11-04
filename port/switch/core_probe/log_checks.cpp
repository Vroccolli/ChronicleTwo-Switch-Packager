#include "log/log.hpp"
#include "log/log_internal.hpp"
#include "kazarov/log.hpp"
#include "platform/paths.hpp"
#include "platform/settings.hpp"
#include "ps2_printf.hpp"
#undef printf

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <mutex>
#include <thread>
#include <vector>

namespace {
struct Line { std::string category; std::string message; };
std::mutex g_mutex;
std::vector<Line> g_lines;
std::string g_path;
void Capture(std::string_view category, LogLevel, std::string_view message) {
    const std::lock_guard lock(g_mutex);
    g_lines.push_back({std::string(category), std::string(message)});
}
bool Has(std::string_view category, std::string_view message) {
    const std::lock_guard lock(g_mutex);
    return std::ranges::any_of(g_lines, [&](const Line &line) { return line.category == category && line.message == message; });
}
void Result(const char *name, bool passed) { std::printf("%-25s %s\n", name, passed ? "PASS" : "FAIL"); }
}

extern "C" int ctSwitchLogChecks(const char *save_root) {
    { const std::lock_guard lock(g_mutex); g_lines.clear(); }
    SettingsSetEnvPrefix("DC2_PROBE");
    LogInit({});
    LogSetCaptureHook(Capture);
    LogConfig config;
    config.console = false;
    config.level = LogLevel::Warning;
    config.categories = {{LogCategory::Data, LogLevel::Debug}};
    LogApplyConfig(config);
    int evaluated = 0;
    CT_LOG_INFO(Main, "hidden {}", ++evaluated);
    CT_LOG_DEBUG(Data, "before file {}", 42);
    const bool filtered = evaluated == 0 && Has("data", "before file 42");

    LogAttachFile(PathsFromUtf8(save_root), config);
    g_path = PathsDisplay(LogFilePath());
    CT_LOG_FILE_INFO("probe session {}", LogSessionId());
    LogInit({.level_override = "error,data=trace"});
    CT_LOG_WARN(Main, "also hidden {}", ++evaluated);
    CT_LOG_TRACE(Data, "category override");
    const bool precedence = evaluated == 0 && Has("data", "category override") && LogEffectiveLevel(LogCategory::Main) == LogLevel::Error;
    LogInit({.level_override = "debug"});
    for (int i = 0; i < 10; ++i) CT_LOG_INFO_LIMIT(std::chrono::hours(1), Movie, "limited {}", i);
    for (int i = 0; i < 10; ++i) CT_LOG_INFO_EVERY_N(5, Movie, "every {}", i);
    const bool rate = Has("movie", "limited 0 (1x)") && Has("movie", "every 0") && Has("movie", "every 5") && !Has("movie", "every 1");
    Ps2PrintfWrite("joined ");
    Ps2PrintfWrite("line\n");
    CtPs2Printf("printf bridge %d\n", 7);
    kz::Log(kz::LogLevel::Warning, "probe", "shared engine sink");
    const bool bridges = Has("ps2", "joined line") && Has("ps2", "printf bridge 7") && Has("kazarov", "probe: shared engine sink");

    std::vector<std::thread> workers;
    for (int worker = 0; worker < 2; ++worker) workers.emplace_back([worker] {
        LogThreadReady();
        for (int i = 0; i < 16; ++i) CT_LOG_INFO(Memory, "worker {} record {}", worker, i);
    });
    for (auto &worker : workers) worker.join();
    bool threads = true;
    for (int worker = 0; worker < 2; ++worker)
        for (int i = 0; i < 16; ++i) threads &= Has("memory", std::format("worker {} record {}", worker, i));
    Ps2PrintfWrite("last partial");
    LogShutdown();
    CT_LOG_WARN(Main, "late record");
    const bool flushed = LogFlushForCrash(std::chrono::milliseconds(50)) && Has("ps2", "last partial") && Has("main", "late record");
    LogFlush();
    std::ifstream input(PathsFromUtf8(g_path), std::ios::binary);
    const std::string file{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const bool persisted = !g_path.empty() && file.find("before file 42") != std::string::npos &&
        file.find("probe session ") != std::string::npos && file.find("worker 1 record 15") != std::string::npos &&
        file.find("last partial") != std::string::npos && file.find("late record") != std::string::npos &&
        file.find("hidden") == std::string::npos;
    LogSetCaptureHook(nullptr);
    config.file = false;
    LogApplyConfig(config);
    LogInit({});
    Result("filtering / formatting", filtered);
    Result("boot/category precedence", precedence);
    Result("rate limits", rate);
    Result("printf / Kazarov bridges", bridges);
    Result("worker threads", threads);
    Result("shutdown / late / flush", flushed);
    Result("session file readback", persisted);
    return (!filtered ? 1 : 0) | (!precedence ? 2 : 0) | (!rate ? 4 : 0) | (!bridges ? 8 : 0) |
        (!threads ? 16 : 0) | (!flushed ? 32 : 0) | (!persisted ? 64 : 0);
}

extern "C" const char *ctSwitchLogPath() { return g_path.c_str(); }
