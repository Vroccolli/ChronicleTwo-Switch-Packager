#pragma once

#include <chrono>
#include <cstdint>
#include <format>
#include <utility>

namespace log_detail {
inline constexpr std::size_t kSessionLogger = static_cast<std::size_t>(LogCategory::Count);
bool SyncEnabled(std::size_t index, LogLevel level);
void SyncWrite(std::size_t index, LogLevel level, std::string_view message);

template <class... Args>
void SyncFormat(std::size_t index, LogLevel level, std::format_string<Args...> format, Args &&...args) {
    SyncWrite(index, level, std::format(format, std::forward<Args>(args)...));
}

struct RateLimit {
    std::chrono::steady_clock::time_point last{};
    std::uint64_t repeats = 0;
    bool sent = false;
};
} // namespace log_detail

#ifndef CT_LOG_SYNC_COMPILE_LEVEL
#define CT_LOG_SYNC_COMPILE_LEVEL 0
#endif
#define CT_LOG_INDEX_(category) static_cast<std::size_t>(::LogCategory::category)
#define CT_LOG_SYNC_ENABLED_(index, level) \
    (static_cast<int>(::LogLevel::level) >= CT_LOG_SYNC_COMPILE_LEVEL && \
     ::log_detail::SyncEnabled(index, ::LogLevel::level))
#define CT_LOG_SYNC_CALL_(index, level, fmt, ...) \
    do { \
        if (CT_LOG_SYNC_ENABLED_(index, level)) \
            ::log_detail::SyncFormat(index, ::LogLevel::level, fmt __VA_OPT__(,) __VA_ARGS__); \
    } while (0)
#define CT_LOG_TRACE(category, fmt, ...) CT_LOG_SYNC_CALL_(CT_LOG_INDEX_(category), Trace, fmt __VA_OPT__(,) __VA_ARGS__)
#define CT_LOG_DEBUG(category, fmt, ...) CT_LOG_SYNC_CALL_(CT_LOG_INDEX_(category), Debug, fmt __VA_OPT__(,) __VA_ARGS__)
#define CT_LOG_INFO(category, fmt, ...) CT_LOG_SYNC_CALL_(CT_LOG_INDEX_(category), Info, fmt __VA_OPT__(,) __VA_ARGS__)
#define CT_LOG_WARN(category, fmt, ...) CT_LOG_SYNC_CALL_(CT_LOG_INDEX_(category), Warning, fmt __VA_OPT__(,) __VA_ARGS__)
#define CT_LOG_ERROR(category, fmt, ...) CT_LOG_SYNC_CALL_(CT_LOG_INDEX_(category), Error, fmt __VA_OPT__(,) __VA_ARGS__)
#define CT_LOG_CRITICAL(category, fmt, ...) CT_LOG_SYNC_CALL_(CT_LOG_INDEX_(category), Critical, fmt __VA_OPT__(,) __VA_ARGS__)
#define CT_LOG_FILE_INFO(fmt, ...) CT_LOG_SYNC_CALL_(::log_detail::kSessionLogger, Info, fmt __VA_OPT__(,) __VA_ARGS__)

#define CT_LOG_SYNC_LIMIT_(level, interval, category, fmt, ...) \
    do { \
        if (CT_LOG_SYNC_ENABLED_(CT_LOG_INDEX_(category), level)) { \
            static thread_local ::log_detail::RateLimit ct_rate_; \
            const auto ct_now_ = std::chrono::steady_clock::now(); \
            ++ct_rate_.repeats; \
            if (!ct_rate_.sent || ct_now_ - ct_rate_.last >= (interval)) { \
                ::log_detail::SyncWrite(CT_LOG_INDEX_(category), ::LogLevel::level, \
                    std::format(fmt __VA_OPT__(,) __VA_ARGS__) + std::format(" ({}x)", ct_rate_.repeats)); \
                ct_rate_.sent = true; ct_rate_.last = ct_now_; ct_rate_.repeats = 0; \
            } \
        } \
    } while (0)
#define CT_LOG_TRACE_LIMIT(interval, category, fmt, ...) CT_LOG_SYNC_LIMIT_(Trace, interval, category, fmt __VA_OPT__(,) __VA_ARGS__)
#define CT_LOG_DEBUG_LIMIT(interval, category, fmt, ...) CT_LOG_SYNC_LIMIT_(Debug, interval, category, fmt __VA_OPT__(,) __VA_ARGS__)
#define CT_LOG_INFO_LIMIT(interval, category, fmt, ...) CT_LOG_SYNC_LIMIT_(Info, interval, category, fmt __VA_OPT__(,) __VA_ARGS__)
#define CT_LOG_WARN_LIMIT(interval, category, fmt, ...) CT_LOG_SYNC_LIMIT_(Warning, interval, category, fmt __VA_OPT__(,) __VA_ARGS__)
#define CT_LOG_ERROR_LIMIT(interval, category, fmt, ...) CT_LOG_SYNC_LIMIT_(Error, interval, category, fmt __VA_OPT__(,) __VA_ARGS__)

#define CT_LOG_SYNC_EVERY_(level, n, category, fmt, ...) \
    do { \
        if (CT_LOG_SYNC_ENABLED_(CT_LOG_INDEX_(category), level)) { \
            static thread_local std::uint64_t ct_count_ = 0; \
            const auto ct_n_ = (n); \
            if (ct_count_++ % (ct_n_ > 0 ? ct_n_ : 1) == 0) \
                ::log_detail::SyncFormat(CT_LOG_INDEX_(category), ::LogLevel::level, fmt __VA_OPT__(,) __VA_ARGS__); \
        } \
    } while (0)
#define CT_LOG_DEBUG_EVERY_N(n, category, fmt, ...) CT_LOG_SYNC_EVERY_(Debug, n, category, fmt __VA_OPT__(,) __VA_ARGS__)
#define CT_LOG_INFO_EVERY_N(n, category, fmt, ...) CT_LOG_SYNC_EVERY_(Info, n, category, fmt __VA_OPT__(,) __VA_ARGS__)
#define CT_LOG_WARN_EVERY_N(n, category, fmt, ...) CT_LOG_SYNC_EVERY_(Warning, n, category, fmt __VA_OPT__(,) __VA_ARGS__)
