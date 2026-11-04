#include <stdio.h>
#include <switch.h>

int ctSwitchMathChecks(void);

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    int failures = ctSwitchMathChecks();
    printf("ChronicleTwo PC-to-Switch: core math probe\n\n");
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
    return failures ? 1 : 0;
}
