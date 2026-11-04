#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
size_t __nx_heap_size = 0;
int ctVulkanProbe(void);
void ctProbeProgress(const char *message) {
    printf("%s\n", message);
    consoleUpdate(NULL);
}
int main(void) {
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad; padInitializeDefault(&pad);
    printf("ChronicleTwo Switch Vulkan capability probe 0.5.0\n");
    printf("Applet type: %u\n", (unsigned)appletGetAppletType());
    int failures = ctVulkanProbe();
    printf("Failure bits: 0x%02x\n", failures);
    printf("This tests the GPU driver and engine requirements.\n");
    printf("Press + to exit.\n");
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return failures != 0;
}
