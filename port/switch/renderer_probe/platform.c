#include <switch.h>
#include <stdio.h>
size_t __nx_heap_size = 0;
static PadState pad;
void ctRendererProbeStart(void) {
    consoleInit(NULL);
    printf("ChronicleTwo real renderer probe 0.6.1\nPreparing SDL and Vulkan...\n");
    consoleUpdate(NULL);
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
}
void ctRendererProbeGraphics(void) { consoleExit(NULL); }
int ctRendererProbeExit(void) {
    padUpdate(&pad);
    return !appletMainLoop() || (padGetButtonsDown(&pad)&HidNpadButton_Plus);
}
