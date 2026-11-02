#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass=0,g_fail=0;
#define RUN(fn) do {printf("RUN  %s\n",#fn);fn();printf("OK   %s\n",#fn);} while(0)
#define ASSERT_TRUE(expr) do {if(!(expr)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#expr);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_default_recipe_runs_core_and_thermal_display(void) {
    FILE *pipe=popen("make -n test 2>&1","r");ASSERT_TRUE(pipe);
    char line[4096];int core=0,thermal=0,overridden=0;
    while(fgets(line,sizeof(line),pipe)) {
        if(strcmp(line,"./build/gpu_tests\n")==0) core=1;
        if(strcmp(line,"./build/thermal_phase_gpu_display_tests\n")==0) thermal=1;
        if(strstr(line,"overriding recipe")||strstr(line,"ignoring old recipe")) overridden=1;
    }
    ASSERT_EQ(pclose(pipe),0);
    ASSERT_TRUE(core&&thermal&&!overridden);PASS();
}
static void test_live_player_binary_builds_its_drive_shader(void) {
    FILE *pipe=popen("make -n build/dungeoncraft_quarter_native 2>&1","r");ASSERT_TRUE(pipe);
    char line[4096];int shader=0;
    while(fgets(line,sizeof(line),pipe))
        if(strstr(line,"glslangValidator")&&strstr(line,"shaders/sim/player_drive.comp")) shader=1;
    ASSERT_EQ(pclose(pipe),0);
    ASSERT_TRUE(shader);PASS();
}
int main(void) {
    printf("=== Default Test Recipe Tests ===\n\n");
    RUN(test_default_recipe_runs_core_and_thermal_display);
    RUN(test_live_player_binary_builds_its_drive_shader);
    printf("\n=== Results: %d passed, %d failed ===\n",g_pass,g_fail);
    return g_fail>0?1:0;
}
