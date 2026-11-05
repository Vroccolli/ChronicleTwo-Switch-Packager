#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

size_t __nx_heap_size = 0;
int ctSwitchVerifyGameArchive(void);
const char *ctSwitchGameBuildId(void) { return "switch-engine-0.7.1"; }
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
    printf("ChronicleTwo native Switch preview 0.7.1\nApplet type: %u\nRun root: %s\n",
        (unsigned)appletGetAppletType(),root);
    // Select SD for POSIX-rooted engine paths. std::filesystem treats sdmc:/
    // as relative and would otherwise prepend the launch working directory.
    if (chdir("sdmc:/") != 0) {
        perror("Cannot select SD root");
        exit(EXIT_FAILURE);
    }
    if (ctSwitchVerifyGameArchive()!=0) {
        fputs("Game archive verification failed; refusing to enter the game\n",stderr);
        exit(EXIT_FAILURE);
    }
}
