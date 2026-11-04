#include "log/log.hpp"
extern "C" int ctSwitchLogChecks(const char *save_root);

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    const int result = ctSwitchLogChecks(argv[1]);
    LogShutdown();
    return result;
}
