#include <stdio.h>
#include <switch.h>

int ctSwitchMathChecks(void);
#ifdef CT_SWITCH_LOG_PROBE
int ctSwitchLogChecks(const char *save_root);
const char *ctSwitchLogPath(void);
#endif

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    int failures = ctSwitchMathChecks();
#ifdef CT_SWITCH_LOG_PROBE
    printf("ChronicleTwo PC-to-Switch: logging probe\n\n");
    int logging_failures = ctSwitchLogChecks("sdmc:/switch/ChronicleTwo-PC-Switch/logging-probe/saves");
    printf("\nLog file: %s\n", ctSwitchLogPath());
    printf("Logging failure bits: 0x%02x\n", logging_failures);
#else
    printf("ChronicleTwo PC-to-Switch: core math probe\n\n");
#endif
    printf("Real PC game math and Kazarov libvu0, compiled for ARM64.\n");
    printf("Normalization, matrix translation, game interpolation: %s\n", failures ? "FAIL" : "PASS");
    printf("Failed comparisons: %d\n\n", failures);
    printf("This diagnostic does not boot the game engine.\n");
    printf("Press + to return to Homebrew Menu.\n");
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
#ifdef CT_SWITCH_LOG_PROBE
    return failures || logging_failures ? 1 : 0;
#else
    return failures ? 1 : 0;
#endif
}
