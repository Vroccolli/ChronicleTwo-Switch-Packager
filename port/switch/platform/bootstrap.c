#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

size_t __nx_heap_size = 0;
void ctSwitchBootstrap(void) {
    const char *root="sdmc:/switch/ChronicleTwo-PC-Switch/game";
    mkdir(root,0777);
    freopen("sdmc:/switch/ChronicleTwo-PC-Switch/game/stdout.log","wb",stdout);
    freopen("sdmc:/switch/ChronicleTwo-PC-Switch/game/stderr.log","wb",stderr);
    setvbuf(stdout,NULL,_IONBF,0);
    setvbuf(stderr,NULL,_IONBF,0);
    // First boot uses the already tested single graphics-thread path.
    // An explicitly supplied setting still takes precedence.
    setenv("DC2_RENDER_THREAD","off",0);
    printf("ChronicleTwo native Switch preview 0.7.0\nApplet type: %u\nRun root: %s\n",
        (unsigned)appletGetAppletType(),root);
}
