#include <switch.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// The host supplies the digest of its already verified, immutable archive.
// Verify the SD bytes locally before the game reads any of them, avoiding a
// second multi-gigabyte trip over FTP just to calculate the same checksum.
int ctSwitchVerifyGameArchive(void) {
    const char *root="/switch/ChronicleTwo-PC-Switch/game/";
    FILE *expected=fopen("/switch/ChronicleTwo-PC-Switch/game/archive.sha256","rb");
    if (!expected) {
        if (errno==ENOENT) {
            puts("No archive.sha256 supplied; archive transfer verification unavailable");
            return 0;
        }
        perror("Cannot open archive.sha256");
        return -1;
    }
    unsigned long long wanted_size=0;
    char wanted[65]={0},extra=0;
    int fields=fscanf(expected,"%llu %64s %c",&wanted_size,wanted,&extra);
    fclose(expected);
    if (fields!=2 || strlen(wanted)!=64 || wanted_size==0 ||
        strspn(wanted,"0123456789abcdef")!=64) {
        fputs("Invalid archive.sha256 manifest\n",stderr);
        return -1;
    }
    FILE *result=fopen("/switch/ChronicleTwo-PC-Switch/game/asset-verification.json","wb");
    if (!result) { perror("Cannot create asset verification report"); return -1; }
    fputs("{\"verified\":false,\"status\":\"running\"}\n",result);
    if (fclose(result)!=0) { perror("Cannot close asset verification report"); return -1; }
    FILE *archive=fopen("/switch/ChronicleTwo-PC-Switch/game/data.zip","rb");
    if (!archive) { perror("Cannot open game archive"); return -1; }
    struct stat info;
    if (fstat(fileno(archive),&info)!=0 || info.st_size<0 || (uint64_t)info.st_size!=wanted_size) {
        fputs("Game archive size mismatch or incomplete upload\n",stderr);
        fclose(archive); return -1;
    }
    unsigned char *buffer=malloc(256*1024);
    if (!buffer) { fclose(archive); fputs("Cannot allocate checksum buffer\n",stderr); return -1; }
    Sha256Context hash; sha256ContextCreate(&hash);
    uint64_t bytes=0,next_progress=256*1024*1024;
    printf("Verifying SD archive at %s (%llu bytes)\n",root,wanted_size);
    size_t count;
    while ((count=fread(buffer,1,256*1024,archive))!=0) {
        sha256ContextUpdate(&hash,buffer,count); bytes+=count;
        if (bytes>=next_progress) {
            printf("Archive checksum: %" PRIu64 "/%llu bytes\n",bytes,wanted_size);
            next_progress+=256*1024*1024;
        }
    }
    int failed=ferror(archive); fclose(archive); free(buffer);
    unsigned char digest[SHA256_HASH_SIZE]; char actual[65];
    sha256ContextGetHash(&hash,digest);
    for (size_t i=0;i<SHA256_HASH_SIZE;++i) sprintf(actual+i*2,"%02x",digest[i]);
    int matches=!failed && bytes==wanted_size && strcmp(actual,wanted)==0;
    result=fopen("/switch/ChronicleTwo-PC-Switch/game/asset-verification.json","wb");
    if (!result) { perror("Cannot save asset verification result"); return -1; }
    fprintf(result,"{\"verified\":%s,\"method\":\"libnx SHA256 on SD\",\"bytes\":%" PRIu64
        ",\"expected_bytes\":%llu,\"sha256\":\"%s\",\"expected_sha256\":\"%s\"}\n",
        matches?"true":"false",bytes,wanted_size,actual,wanted);
    int write_failed=ferror(result);
    if (fclose(result)!=0) write_failed=1;
    printf("SD archive checksum: %s (%s)\n",matches?"PASS":"FAIL",actual);
    return matches && !write_failed ? 0 : -1;
}
