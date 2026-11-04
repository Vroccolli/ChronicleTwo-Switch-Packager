#include <stdio.h>
#include <switch.h>

int ctSwitchMathChecks(void);
#ifdef CT_SWITCH_SERVICES_PROBE
int ctSwitchServicesChecks(void);
#endif
#ifdef CT_SWITCH_LOG_PROBE
int ctSwitchLogChecks(const char *save_root);
const char *ctSwitchLogPath(void);
#endif
#ifdef CT_SWITCH_RUNTIME_PROBE
int ctSwitchRuntimeChecks(void);
void ctProbeProgress(const char *message) {
    printf("%s\n", message);
    consoleUpdate(NULL);
}
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
#ifdef CT_SWITCH_RUNTIME_PROBE
#ifdef CT_SWITCH_SERVICES_PROBE
    printf("ChronicleTwo PC-to-Switch: services probe 0.4.1\n\n");
    int logging_failures = ctSwitchLogChecks("sdmc:/switch/ChronicleTwo-PC-Switch/services-probe/saves");
#else
    printf("ChronicleTwo PC-to-Switch: runtime probe 0.3.0\n\n");
    int logging_failures = ctSwitchLogChecks("sdmc:/switch/ChronicleTwo-PC-Switch/runtime-probe/saves");
#endif
    int runtime_failures = ctSwitchRuntimeChecks();
#ifdef CT_SWITCH_SERVICES_PROBE
    int services_failures = ctSwitchServicesChecks();
    consoleClear();
    printf("ChronicleTwo PC-to-Switch: services probe 0.4.1\n\n");
    printf("Math: %s\nLogging (7 checks): %s\nRuntime (8 checks): %s\n",
        failures ? "FAIL" : "PASS", logging_failures ? "FAIL" : "PASS", runtime_failures ? "FAIL" : "PASS");
    printf("Report file readback: %s\n", (services_failures & 1) ? "FAIL" : "PASS");
    printf("Context / path redaction: %s\n", (services_failures & 2) ? "FAIL" : "PASS");
    printf("Raw frames / module offsets: %s\n", (services_failures & 4) ? "FAIL" : "PASS");
    printf("Services failure bits: 0x%02x\n", services_failures);
    printf("Controlled report only; raw CPU faults use system handler.\n");
    printf("Report: services-probe/saves/crashes/switch-last-report.txt\n");
#endif
    printf("Runtime failure bits: 0x%02x\n", runtime_failures);
#else
    printf("ChronicleTwo PC-to-Switch: logging probe\n\n");
    int logging_failures = ctSwitchLogChecks("sdmc:/switch/ChronicleTwo-PC-Switch/logging-probe/saves");
#endif
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
#ifdef CT_SWITCH_RUNTIME_PROBE
#ifdef CT_SWITCH_SERVICES_PROBE
    return failures || logging_failures || runtime_failures || services_failures ? 1 : 0;
#else
    return failures || logging_failures || runtime_failures ? 1 : 0;
#endif
#else
    return failures || logging_failures ? 1 : 0;
#endif
#else
    return failures ? 1 : 0;
#endif
}
