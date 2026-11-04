#include <gtest/gtest.h>
#include "log/log.hpp"

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    LogShutdown();
    return result;
}
