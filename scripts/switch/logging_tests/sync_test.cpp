#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include "log/log.hpp"
#include "log/log_internal.hpp"
#include "platform/settings.hpp"

TEST(SyncLog, CrashFlushBeforeInitializationDoesNotStartLogging) {
    EXPECT_FALSE(LogFlushForCrash(std::chrono::milliseconds(1)));
    LogInit({});
    EXPECT_TRUE(LogFlushForCrash(std::chrono::milliseconds(10)));
}

TEST(SyncLog, FilteredArgumentsAreNotEvaluated) {
    LogInit({.level_override = "error"});
    int evaluated = 0;
    CT_LOG_INFO(Main, "filtered {}", ++evaluated);
    EXPECT_EQ(evaluated, 0);
    LogInit({});
}

TEST(SyncLog, CaptureCanQueryAndFlushWithoutRecursiveLogging) {
    LogConfig config;
    config.file = false;
    config.console = false;
    LogApplyConfig(config);
    static int captured;
    captured = 0;
    LogSetCaptureHook([](std::string_view, LogLevel, std::string_view) {
        ++captured;
        EXPECT_TRUE(LogIsBackendThread());
        EXPECT_FALSE(LogFlushForCrash(std::chrono::milliseconds(1)));
        LogFlush();
        CT_LOG_WARN(Main, "recursive capture line");
    });
    CT_LOG_WARN(Main, "outer line");
    EXPECT_EQ(captured, 1);
    LogSetCaptureHook(nullptr);
}

TEST(SyncLog, ShutdownPreservesLateRecordsAndFlushesPartialPrintf) {
    static std::string text;
    text.clear();
    LogInit({.level_override = "debug"});
    LogSetCaptureHook([](std::string_view, LogLevel, std::string_view line) { text += std::string(line) + '\n'; });
    Ps2PrintfWrite("partial");
    LogShutdown();
    CT_LOG_WARN(Main, "after shutdown {}", 7);
    EXPECT_EQ(text, "partial\nafter shutdown 7\n");
    EXPECT_TRUE(LogFlushForCrash(std::chrono::milliseconds(10)));
    LogSetCaptureHook(nullptr);
    LogInit({});
}

TEST(SyncLog, FileOpenFailureKeepsConsoleLoggingAvailable) {
    const auto blocked = std::filesystem::temp_directory_path() /
        std::format("ct-log-file-{}", std::chrono::steady_clock::now().time_since_epoch().count());
    { std::ofstream file(blocked); file << "not a directory"; }
    LogConfig config;
    config.file = true;
    config.console = true;
    testing::internal::CaptureStderr();
    LogAttachFile(blocked / "saves", config);
    EXPECT_TRUE(LogFilePath().empty());
    CT_LOG_ERROR(Main, "console survives file failure");
    LogFlush();
    const std::string output = testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("could not open"), std::string::npos);
    EXPECT_NE(output.find("console survives file failure\n"), std::string::npos);
    config.file = false;
    LogApplyConfig(config);
    std::filesystem::remove(blocked);
}
