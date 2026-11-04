#include <gtest/gtest.h>
extern "C" void ctRuntimeTestInit();
int main(int argc, char **argv) {
    ctRuntimeTestInit();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
