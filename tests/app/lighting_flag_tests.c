#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

static int g_pass, g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_default_albedo_and_opt_in_lighting_in_actual_window(void) {
    char directory[]="build/lighting_flag_XXXXXX";
    ASSERT_TRUE(mkdtemp(directory));
    printf("Graphical evidence: %s\n",directory);
    uint64_t hashes[2]={0};
    for(unsigned enabled=0;enabled<2;enabled++) {
        unsigned lit=1u-enabled;
        char command[2048],path[512],log[8192];
        snprintf(command,sizeof(command),
            "SDL_VIDEODRIVER=wayland ./tools/with-vulkan ./build/dungeoncraft_360 %s "
            "--scene assets/props/level_piece03.prop/asset.cprop --world-dir %s/world%u "
            "--benchmark-frames 1 --benchmark-capture %s/frame%u.bmp > %s/run%u.log 2>&1",
            lit?"--lighting":"",directory,lit,directory,lit,directory,lit);
        int status=system(command);
        ASSERT_TRUE(status!=-1&&WIFEXITED(status));
        snprintf(path,sizeof(path),"%s/run%u.log",directory,lit);
        FILE *file=fopen(path,"rb");ASSERT_TRUE(file);
        size_t bytes=fread(log,1,sizeof(log)-1,file);log[bytes]=0;fclose(file);
        ASSERT_TRUE(strstr(log,"Four paused pixels identical"));
        ASSERT_TRUE(strstr(log,lit?"Propagated lighting: enabled":"Propagated lighting: disabled"));
        ASSERT_TRUE(WEXITSTATUS(status)==0||
            (WEXITSTATUS(status)==1&&strstr(log,"Displayed total frame budget exceeds 30ms")));
        snprintf(path,sizeof(path),"%s/frame%u.bmp",directory,lit);
        file=fopen(path,"rb");ASSERT_TRUE(file);
        uint64_t hash=UINT64_C(1469598103934665603);int byte;
        while((byte=fgetc(file))!=EOF) hash=(hash^(unsigned)byte)*UINT64_C(1099511628211);
        fclose(file);hashes[lit]=hash;
    }
    ASSERT_TRUE(hashes[0]!=hashes[1]);
    PASS();
}
int main(void) {
    RUN(test_default_albedo_and_opt_in_lighting_in_actual_window);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail!=0;
}
