#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static uint32_t workgroup_variables(const char *path) {
    FILE *file=fopen(path,"rb");if(!file) return UINT32_MAX;
    if(fseek(file,0,SEEK_END)!=0) {fclose(file);return UINT32_MAX;}
    long bytes=ftell(file);rewind(file);
    if(bytes<20||bytes%4) {fclose(file);return UINT32_MAX;}
    uint32_t *words=malloc((size_t)bytes);if(!words) {fclose(file);return UINT32_MAX;}
    bool okay=fread(words,1,(size_t)bytes,file)==(size_t)bytes;fclose(file);
    uint32_t count=0;
    if(!okay||words[0]!=0x07230203u) count=UINT32_MAX;
    else for(size_t at=5;at<(size_t)bytes/4;) {
        uint32_t length=words[at]>>16u,opcode=words[at]&65535u;
        if(!length||at+length>(size_t)bytes/4) {count=UINT32_MAX;break;}
        if(opcode==59u&&length>=4u&&words[at+3u]==4u) count++;
        at+=length;
    }
    free(words);return count;
}
static void test_production_mpm_does_not_reserve_experimental_shared_memory(void) {
    uint32_t normal=workgroup_variables("build/shaders/mpm.comp.spv");
    uint32_t diagnostic=workgroup_variables("build/diagnostics/mpm_gather_profile.comp.spv");
    printf("production workgroup variables%u diagnostic%u\n",normal,diagnostic);
    ASSERT_EQ(normal,0u);ASSERT_TRUE(diagnostic>0u&&diagnostic!=UINT32_MAX);PASS();
}
int main(void) {
    RUN(test_production_mpm_does_not_reserve_experimental_shared_memory);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
