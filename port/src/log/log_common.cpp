#include "log/log.hpp"
#include "log/log_internal.hpp"
#include "platform/paths.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <mutex>
#include <system_error>

namespace {
constexpr std::size_t kCategoryCount = static_cast<std::size_t>(LogCategory::Count);
constexpr std::string_view kLogPrefix = "darkchronicle-";
constexpr std::string_view kLogSuffix = ".log";
std::mutex g_ps2_mutex;
std::string g_ps2_pending;
/** Lowercase names used for category configuration and file records. */
constexpr std::array<std::string_view, kCategoryCount> kCategoryNames = {
    "main",
    "config",
    "data",
    "gfx",
    "audio",
    "input",
    "save",
    "menu",
    "event",
    "dungeon",
    "town",
    "movie",
    "script",
    "memory",
    "ps2",
    "kazarov",
    "crash",
};
/** Lowercase names accepted in severity configuration. */
constexpr std::array<std::string_view, 7> kLevelNames = {"trace", "debug", "info", "warning", "error", "critical", "off"};
std::string Lower(std::string_view text) {
    std::string lower(text);
    std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

std::string_view Trim(std::string_view text) {
    constexpr std::string_view space = " \t";
    const std::size_t          first = text.find_first_not_of(space);
    if (first == std::string_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(space) - first + 1);
}

} // namespace

std::string_view LogCategoryName(LogCategory category) {
    return kCategoryNames[static_cast<std::size_t>(category)];
}

std::string_view LogLevelName(LogLevel level) {
    return kLevelNames[static_cast<std::size_t>(level)];
}

std::optional<LogCategory> LogCategoryFromName(std::string_view name) {
    const std::string lower = Lower(name);
    for (std::size_t i = 0; i < kCategoryCount; ++i) {
        if (lower == kCategoryNames[i]) {
            return static_cast<LogCategory>(i);
        }
    }
    return std::nullopt;
}

std::optional<LogLevel> LogLevelFromName(std::string_view name) {
    const std::string lower = Lower(name);
    if (lower == "warn") {
        return LogLevel::Warning;
    }
    for (std::size_t i = 0; i < kLevelNames.size(); ++i) {
        if (lower == kLevelNames[i]) {
            return static_cast<LogLevel>(i);
        }
    }
    return std::nullopt;
}

std::optional<LogSpec> LogParseSpec(std::string_view spec) {
    LogSpec result;
    if (Trim(spec).empty()) {
        return std::nullopt;
    }
    for (bool more = true; more;) {
        const std::size_t      comma = spec.find(',');
        const std::string_view item = Trim(spec.substr(0, comma));
        more = comma != std::string_view::npos;
        spec = more ? spec.substr(comma + 1) : std::string_view{};
        const std::size_t equals = item.find('=');
        if (equals == std::string_view::npos) {
            const std::optional<LogLevel> level = LogLevelFromName(item);
            if (!level) {
                return std::nullopt;
            }
            result.level = level;
            continue;
        }
        const std::optional<LogCategory> category = LogCategoryFromName(Trim(item.substr(0, equals)));
        const std::optional<LogLevel>    level = LogLevelFromName(Trim(item.substr(equals + 1)));
        if (!category || !level) {
            return std::nullopt;
        }
        result.categories.emplace_back(*category, *level);
    }
    return result;
}

bool LogSpecValid(std::string_view spec) {
    return LogParseSpec(spec).has_value();
}

LogLevel LogResolveLevel(LogCategory category, const LogConfig &config, const LogSpec &boot) {
    const auto find = [category](const std::vector<std::pair<LogCategory, LogLevel> > &levels) {
        const auto it = std::ranges::find_if(levels, [category](const auto &entry) { return entry.first == category; });
        return it == levels.end() ? std::nullopt : std::optional(it->second);
    };
    if (const std::optional<LogLevel> level = find(boot.categories)) {
        return *level;
    }
    if (boot.level) {
        return *boot.level;
    }
    if (const std::optional<LogLevel> level = find(config.categories)) {
        return *level;
    }
    return config.level.value_or(kLogLevelDefault);
}

void LogPruneDirectory(const std::filesystem::path &directory, int keep, const std::filesystem::path &current) {
    std::error_code                    error;
    std::vector<std::filesystem::path> logs;
    for (const auto &entry : std::filesystem::directory_iterator(directory, error)) {
        const std::string name = PathsDisplay(entry.path().filename());
        if (name.starts_with(kLogPrefix) && name.ends_with(kLogSuffix) && entry.path() != current &&
            entry.is_regular_file(error)) {
            logs.push_back(entry.path());
        }
    }
    // Session ids start with the date and time, so names sort oldest first.
    std::ranges::sort(logs, std::greater{});
    const std::size_t others = static_cast<std::size_t>(std::max(keep, 1) - (current.empty() ? 0 : 1));
    for (std::size_t i = others; i < logs.size(); ++i) {
        std::filesystem::remove(logs[i], error);
    }
}

void Ps2PrintfWrite(std::string_view text) {
    const std::scoped_lock lock(g_ps2_mutex);
    while (!text.empty()) {
        const std::size_t newline = text.find('\n');
        if (newline == std::string_view::npos) {
            g_ps2_pending.append(text);
            return;
        }
        g_ps2_pending.append(text.substr(0, newline));
        text.remove_prefix(newline + 1);
        if (g_ps2_pending.ends_with('\r')) {
            g_ps2_pending.pop_back();
        }
        CT_LOG_DEBUG(Ps2, "{}", g_ps2_pending);
        g_ps2_pending.clear();
    }
}

void Ps2PrintfFlush() {
    const std::scoped_lock lock(g_ps2_mutex);
    if (!g_ps2_pending.empty()) {
        CT_LOG_DEBUG(Ps2, "{}", g_ps2_pending);
        g_ps2_pending.clear();
    }
}
