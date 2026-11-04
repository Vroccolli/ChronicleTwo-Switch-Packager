#if defined(__SWITCH__) || defined(CT_LOG_SYNC)
#include "log/log.hpp"
#include "log/log_internal.hpp"
#include "kazarov/log.hpp"
#include "platform/paths.hpp"
#include "platform/settings.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <ctime>
#include <iterator>
#include <mutex>
#include <system_error>
#include <thread>

namespace {
constexpr std::size_t kEarlyBytes = 1 << 20;
constexpr std::uint64_t kSoftCapBytes = 256ull << 20;
thread_local bool t_delivering = false;

struct State {
    std::timed_mutex mutex;
    LogConfig config;
    LogSpec boot;
    LogCaptureHook capture = nullptr;
    std::filesystem::path save_root;
    std::filesystem::path path;
    std::FILE *file = nullptr;
    std::string early;
    std::uint64_t bytes = 0;
    bool attached = false;
    bool buffering = true;
    bool capped = false;
};
std::atomic<State *> g_state{nullptr};

State &Get() {
    // Late records from thread/static destructors must never touch a destroyed mutex or file sink.
    static State *state = [] {
        auto *created = new State;
        g_state.store(created, std::memory_order_release);
        return created;
    }();
    return *state;
}

bool Enabled(const State &state, std::size_t index, LogLevel level) {
    if (index > log_detail::kSessionLogger || level >= LogLevel::Off) return false;
    return level >= (index == log_detail::kSessionLogger ? LogLevel::Info :
        LogResolveLevel(static_cast<LogCategory>(index), state.config, state.boot));
}

std::string FileLine(LogLevel level, std::string_view category, std::string_view message) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    char date[32]{};
    std::strftime(date, sizeof date, "%Y-%m-%d %H:%M:%S", &local);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    constexpr std::string_view names[] = {"TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "CRIT ", "OFF  "};
    return std::format("{}.{:03} {} [{}] {:<8} {}\n", date, millis, names[static_cast<std::size_t>(level)],
        std::hash<std::thread::id>{}(std::this_thread::get_id()), category, message);
}

void Append(State &state, std::string_view line) {
    if (state.file) {
        if (std::fwrite(line.data(), 1, line.size(), state.file) != line.size()) {
            std::fclose(state.file);
            state.file = nullptr;
            state.path.clear();
            state.buffering = false;
            std::fputs("log: session file write failed; console logging continues\n", stderr);
        }
    } else if (state.buffering && state.early.size() + line.size() <= kEarlyBytes) {
        state.early.append(line);
    }
    state.bytes += line.size();
}

void WriteFile(State &state, LogLevel level, std::string_view category, std::string_view message) {
    if (!state.file && !state.buffering) return;
    if (state.capped && level < LogLevel::Warning) return;
    const std::string line = FileLine(level, category, message);
    if (!state.capped && state.bytes + line.size() > kSoftCapBytes) {
        state.capped = true;
        Append(state, FileLine(LogLevel::Warning, "session", "log: the file reached 256 MiB; only warnings and errors follow"));
        if (level < LogLevel::Warning) return;
    }
    Append(state, line);
}

void ApplyFile(State &state) {
    if (!state.attached) return;
    const char *env = SettingsEnv("LOG_FILE");
    const bool wanted = state.config.file && (env == nullptr || std::string_view(env) != "0") && !state.save_root.empty();
    if (!wanted) {
        if (state.file) std::fclose(state.file);
        state.file = nullptr;
        state.path.clear();
        state.early.clear();
        state.buffering = false;
        return;
    }
    if (state.file) return;
    const auto directory = (state.save_root / ".." / "logs").lexically_normal();
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const auto path = directory / std::format("darkchronicle-{}.log", LogSessionId());
#ifdef _WIN32
    state.file = _wfopen(path.c_str(), L"ab");
#else
    state.file = std::fopen(path.c_str(), "ab");
#endif
    if (!state.file) {
        state.early.clear();
        state.buffering = false;
        std::fprintf(stderr, "log: could not open %s; logging to stderr only\n", PathsDisplay(path).c_str());
        return;
    }
    state.path = path;
    if (!state.early.empty()) {
        const std::string pending = std::move(state.early);
        // These bytes were already counted when the records were buffered.
        const auto previous_bytes = state.bytes;
        Append(state, pending);
        state.bytes = previous_bytes;
    }
    state.early.clear();
    state.buffering = false;
    if (state.file) std::fflush(state.file);
    LogPruneDirectory(directory, state.config.keep, state.path);
}

void KazarovSink(kz::LogLevel level, std::string_view subsystem, std::string_view message) {
    constexpr LogLevel levels[] = {LogLevel::Debug, LogLevel::Info, LogLevel::Warning, LogLevel::Error, LogLevel::Critical};
    const auto mapped = levels[static_cast<std::size_t>(level)];
    if (log_detail::SyncEnabled(static_cast<std::size_t>(LogCategory::Kazarov), mapped))
        log_detail::SyncWrite(static_cast<std::size_t>(LogCategory::Kazarov), mapped, std::format("{}: {}", subsystem, message));
    if (mapped >= LogLevel::Error) LogFlush();
}
} // namespace

bool log_detail::SyncEnabled(std::size_t index, LogLevel level) {
    if (t_delivering) return false;
    auto &state = Get();
    const std::lock_guard lock(state.mutex);
    return Enabled(state, index, level);
}

void log_detail::SyncWrite(std::size_t index, LogLevel level, std::string_view message) {
    if (t_delivering || index > kSessionLogger || level >= LogLevel::Off) return;
    auto &state = Get();
    LogCaptureHook capture;
    const auto category = index == kSessionLogger ? std::string_view("session") : LogCategoryName(static_cast<LogCategory>(index));
    {
        const std::lock_guard lock(state.mutex);
        if (!Enabled(state, index, level)) return;
        if (state.config.console && index != kSessionLogger && index != static_cast<std::size_t>(LogCategory::Ps2)) {
            const std::string line = std::string(message) + '\n';
            std::fwrite(line.data(), 1, line.size(), stderr);
        }
        WriteFile(state, level, category, message);
        capture = state.capture;
    }
    // Capture hooks may query/flush the logger; do not hold its I/O mutex while calling them.
    if (capture) {
        struct Delivery { Delivery() { t_delivering = true; } ~Delivery() { t_delivering = false; } } delivery;
        capture(category, level, message);
    }
}

void LogInit(const LogBootOptions &options) {
    LogSpec boot;
    bool valid = true;
    if (options.level_override && *options.level_override) {
        if (auto parsed = LogParseSpec(options.level_override)) boot = std::move(*parsed);
        else valid = false;
    }
    auto &state = Get();
    { const std::lock_guard lock(state.mutex); state.boot = std::move(boot); }
    kz::SetLogSink(KazarovSink);
    if (!valid) CT_LOG_WARN(Main, "log: ignoring the log level \"{}\"", options.level_override);
}

void LogAttachFile(const std::filesystem::path &save_root, const LogConfig &config) {
    auto &state = Get();
    const std::lock_guard lock(state.mutex);
    state.save_root = save_root;
    state.config = config;
    state.attached = true;
    ApplyFile(state);
}

void LogApplyConfig(const LogConfig &config) {
    auto &state = Get();
    const std::lock_guard lock(state.mutex);
    state.config = config;
    ApplyFile(state);
}

LogLevel LogEffectiveLevel(LogCategory category) {
    auto &state = Get();
    const std::lock_guard lock(state.mutex);
    return LogResolveLevel(category, state.config, state.boot);
}
bool LogConsoleEnabled() {
    auto &state = Get();
    const std::lock_guard lock(state.mutex);
    return state.config.console;
}
void LogSetCaptureHook(LogCaptureHook hook) {
    auto &state = Get();
    const std::lock_guard lock(state.mutex);
    state.capture = hook;
}
void LogFlush() {
    auto &state = Get();
    const std::lock_guard lock(state.mutex);
    std::fflush(stderr);
    if (state.file) std::fflush(state.file);
}
bool LogFlushForCrash(std::chrono::milliseconds budget) {
    if (t_delivering) return false;
    // An early crash must not allocate or lazily initialize the logger.
    State *state = g_state.load(std::memory_order_acquire);
    if (!state) return false;
    std::unique_lock lock(state->mutex, std::defer_lock);
    if (!lock.try_lock_for(std::max(budget, std::chrono::milliseconds::zero()))) return false;
    return std::fflush(stderr) == 0 && (!state->file || std::fflush(state->file) == 0);
}
bool LogIsBackendThread() { return t_delivering; }
void LogShutdown() {
    Ps2PrintfFlush();
    LogFlush();
}
void LogThreadReady() { static_cast<void>(Get()); }
const std::filesystem::path &LogFilePath() { return Get().path; }
std::string_view LogSessionId() {
    static const std::string id = [] {
        const auto now = std::time(nullptr);
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        char text[32]{};
        std::strftime(text, sizeof text, "%Y%m%d-%H%M%S", &local);
        const auto suffix = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()) & 0xffff;
        return std::format("{}-{:04x}", text, suffix);
    }();
    return id;
}
#endif
