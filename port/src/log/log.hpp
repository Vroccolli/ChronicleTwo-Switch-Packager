#pragma once

// Shared logging API. Desktop keeps deferred Quill queues; Switch uses synchronous delivery.

#include <chrono>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

#include "log/log_config.hpp"

/**
 * Startup settings available before the game configuration is loaded.
 */
struct LogBootOptions {
    bool        headless = false;         /**< Whether startup runs without a visible window. */
    const char *level_override = nullptr; /**< Threshold specification from the command line or environment. */
};

/**
 * Starts console logging and creates the process session identifier.
 */
void LogInit(const LogBootOptions &options);
/**
 * Opens the session log beside the save root and applies retention settings.
 */
void LogAttachFile(const std::filesystem::path &save_root, const LogConfig &config);
/**
 * Applies updated console and category logging settings.
 */
void LogApplyConfig(const LogConfig &config);
/**
 * Waits until all previously queued diagnostic records are written.
 */
void LogFlush();
/**
 * Flushes queued records within a crash handler's time budget.
 */
bool LogFlushForCrash(std::chrono::milliseconds budget);
/**
 * Reports whether the caller runs on the logging backend thread.
 */
bool LogIsBackendThread();
/**
 * Drains pending records and stops the logging backend.
 */
void LogShutdown();
/**
 * Returns the identifier shared by all logs from this process.
 */
std::string_view LogSessionId();
/**
 * Returns the active session log path when a file is attached.
 */
const std::filesystem::path &LogFilePath();
/**
 * Prepares the caller's queue for later logging without allocation.
 */
void LogThreadReady();
/**
 * Checks whether a logging threshold specification is valid.
 */
bool LogSpecValid(std::string_view spec);

// Select a backend without exposing desktop allocation headers to Switch consumers.
#if defined(__SWITCH__) || defined(CT_LOG_SYNC)
#include "log/log_sync.hpp"
#else
#include "log/log_quill.hpp"
#endif
