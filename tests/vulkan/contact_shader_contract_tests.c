/* CPU-only contact shader ABI regression checks. */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "../../src/vulkan/contact_shader_contract.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

/* Metadata fixtures are never passed to a Vulkan driver. */
static const uint32_t valid[]={0x07230203u,0x00010600u,0,64,0,
    (4u<<16)|71u,11,1,1,
    (4u<<16)|71u,12,1,31,
    (4u<<16)|71u,20,33,27,
    (3u<<16)|49u,2,11,
    (4u<<16)|50u,3,12,15,
    (4u<<16)|71u,13,1,32,
    (4u<<16)|50u,3,13,DC_CONTACT_WORK_ABI};
static const char *fixture="build/diagnostics/contact_contract_fixture.spv";

static void write_fixture(const uint32_t *words,size_t count) {
    FILE *file=fopen(fixture,"wb");ASSERT_TRUE(file);
    ASSERT_EQ(fwrite(words,sizeof(*words),count,file),count);
    ASSERT_EQ(fclose(file),0);
}
static void test_current_dispatch_contract_is_accepted(void) {
    write_fixture(valid,sizeof(valid)/4);char err[256]={0};
    ASSERT_TRUE(dc_gpu_contact_shader_compatible(fixture,15,err,sizeof(err)));PASS();
}
static void test_runtime_shader_has_the_same_contract(void) {
    char err[256]={0};
    ASSERT_TRUE(dc_gpu_contact_shader_compatible("build/shaders/contacts.comp.spv",15,err,sizeof(err)));PASS();
}
static void test_scratch_offset_mismatch_is_rejected(void) {
    write_fixture(valid,sizeof(valid)/4);char err[256]={0};
    ASSERT_TRUE(!dc_gpu_contact_shader_compatible(fixture,16,err,sizeof(err)));
    ASSERT_TRUE(err[0]!=0);PASS();
}
static void test_forced_pixel_default_is_rejected(void) {
    uint32_t words[sizeof(valid)/4];memcpy(words,valid,sizeof(valid));words[17]=(3u<<16)|48u;
    write_fixture(words,sizeof(words)/4);char err[256]={0};
    ASSERT_TRUE(!dc_gpu_contact_shader_compatible(fixture,15,err,sizeof(err)));PASS();
}
static void test_old_unversioned_or_missing_binding_shaders_are_rejected(void) {
    uint32_t words[sizeof(valid)/4];char err[256]={0};
    memcpy(words,valid,sizeof(valid));words[12]=30;
    write_fixture(words,sizeof(words)/4);
    ASSERT_TRUE(!dc_gpu_contact_shader_compatible(fixture,15,err,sizeof(err)));
    memcpy(words,valid,sizeof(valid));words[16]=26;
    write_fixture(words,sizeof(words)/4);
    ASSERT_TRUE(!dc_gpu_contact_shader_compatible(fixture,15,err,sizeof(err)));PASS();
}
static void test_truncated_and_zero_length_instructions_are_rejected(void) {
    write_fixture(valid,sizeof(valid)/4-1);char err[256]={0};
    ASSERT_TRUE(!dc_gpu_contact_shader_compatible(fixture,15,err,sizeof(err)));
    uint32_t words[sizeof(valid)/4];memcpy(words,valid,sizeof(valid));words[5]=71;
    write_fixture(words,sizeof(words)/4);
    ASSERT_TRUE(!dc_gpu_contact_shader_compatible(fixture,15,err,sizeof(err)));PASS();
}
static void test_unbatched_and_wrong_work_interfaces_are_rejected(void) {
    char err[256]={0};
    write_fixture(valid,sizeof(valid)/4-8);
    ASSERT_TRUE(!dc_gpu_contact_shader_compatible(fixture,15,err,sizeof(err)));
    uint32_t words[sizeof(valid)/4];memcpy(words,valid,sizeof(valid));
    words[sizeof(words)/4-1]=1;
    write_fixture(words,sizeof(words)/4);
    ASSERT_TRUE(!dc_gpu_contact_shader_compatible(fixture,15,err,sizeof(err)));PASS();
}
int main(void) {
    if(mkdir("build/diagnostics",0777)!=0&&errno!=EEXIST) return 2;
    RUN(test_current_dispatch_contract_is_accepted);RUN(test_runtime_shader_has_the_same_contract);
    RUN(test_scratch_offset_mismatch_is_rejected);RUN(test_forced_pixel_default_is_rejected);
    RUN(test_old_unversioned_or_missing_binding_shaders_are_rejected);
    RUN(test_truncated_and_zero_length_instructions_are_rejected);
    RUN(test_unbatched_and_wrong_work_interfaces_are_rejected);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
