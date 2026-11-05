#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

size_t __nx_heap_size = 0;
static PrintConsole *loading_console;
static u64 loading_started;
static void LoadingText(int row,const char *text,u16 color) {
    const size_t length=strlen(text);
    int x=(loading_console->consoleWidth-(int)length)/2;
    loading_console->fg=color;
    for(size_t i=0;i<length && x+(int)i<loading_console->consoleWidth;++i) {
        int glyph=(unsigned char)text[i]-loading_console->font.asciiOffset;
        if(x+(int)i>=0 && glyph>=0 && glyph<loading_console->font.numChars)
            loading_console->renderer->drawChar(loading_console,x+(int)i,row,glyph);
    }
}
void ctSwitchLoadingStage(const char *stage) {
    FILE *status=fopen("sdmc:/switch/ChronicleTwo-PC-Switch/game/startup-stage.txt","wb");
    if(status) {
        fprintf(status,"ChronicleTwo 0.7.13\n%s\nElapsed: %llu ms\n",stage,
            (unsigned long long)(armTicksToNs(armGetSystemTick()-loading_started)/1000000));
        fclose(status);
    }
    if(!loading_console || !loading_console->consoleInitialised)return;
    loading_console->flags=CONSOLE_FG_CUSTOM|CONSOLE_BG_CUSTOM;
    loading_console->bg=RGB565_FROM_RGB8(8,12,24);
    loading_console->fg=RGB565_FROM_RGB8(220,228,240);
    for(int y=0;y<loading_console->consoleHeight;++y)
        for(int x=0;x<loading_console->consoleWidth;++x)
            loading_console->renderer->drawChar(loading_console,x,y,' '-loading_console->font.asciiOffset);
    LoadingText(13,"CHRONICLETWO",RGB565_FROM_RGB8(88,200,240));
    LoadingText(16,"----------------------------------------",RGB565_FROM_RGB8(48,88,120));
    LoadingText(21,"Loading...",RGB565_FROM_RGB8(240,244,252));
    LoadingText(25,stage,RGB565_FROM_RGB8(184,200,220));
    LoadingText(33,"Please wait",RGB565_FROM_RGB8(128,148,176));
    LoadingText(41,"Switch preview 0.7.13",RGB565_FROM_RGB8(100,120,148));
    consoleUpdate(loading_console);
}
void ctSwitchLoadingFinished(void) {
    if(loading_console)consoleExit(loading_console);
    loading_console=NULL;
}
const char *ctSwitchGameBuildId(void) { return "switch-engine-0.7.13"; }
void ctSwitchBootstrap(void) {
    const char *root="sdmc:/switch/ChronicleTwo-PC-Switch/game";
    mkdir(root,0777);
    loading_started=armGetSystemTick();
    loading_console=consoleInit(NULL);
    atexit(ctSwitchLoadingFinished);
    ctSwitchLoadingStage("Preparing your adventure");
    freopen("sdmc:/switch/ChronicleTwo-PC-Switch/game/stdout.log","wb",stdout);
    freopen("sdmc:/switch/ChronicleTwo-PC-Switch/game/stderr.log","wb",stderr);
    setvbuf(stdout,NULL,_IONBF,0);
    setvbuf(stderr,NULL,_IONBF,0);
    // First boot uses the already tested single graphics-thread path.
    // An explicitly supplied setting still takes precedence.
    setenv("DC2_RENDER_THREAD","off",0);
    printf("ChronicleTwo native Switch preview 0.7.13\nApplet type: %u\nRun root: %s\n",
        (unsigned)appletGetAppletType(),root);
    // Select SD for POSIX-rooted engine paths. std::filesystem treats sdmc:/
    // as relative and would otherwise prepend the launch working directory.
    if (chdir("sdmc:/") != 0) {
        perror("Cannot select SD root");
        exit(EXIT_FAILURE);
    }
}
