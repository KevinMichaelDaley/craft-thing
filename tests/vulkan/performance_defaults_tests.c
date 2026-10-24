#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void profile_case(bool defaults) {
    const char *flags[]={"DC_MPM_PARTICLE_SWAP","DC_MPM_KERNELS","DC_MPM_GATHER_STENCIL",
        "DC_MPM_FUSED_GRID","DC_SOLVER_APPLY_OVERLAP","DC_SOLVER_SKIP_ZERO_REDUCTIONS","DC_CONTACT_BODY_CACHE"};
    for(unsigned i=0;i<sizeof(flags)/sizeof(*flags);i++) {
        if(defaults) ASSERT_EQ(unsetenv(flags[i]),0);
        else ASSERT_EQ(setenv(flags[i],i==1u?"generic":i==6u?"1":"0",1),0);
    }
    dc_gpu_t *g=NULL;char err[256]={0};
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    bool correct=g->mpm_swap_buffers==defaults&&g->mpm_fused_grid==defaults&&
        g->solver_apply_overlap==defaults&&g->solver_skip_zero_reductions==defaults;
    correct=correct&&((g->mpm_swap_commit_pipeline!=VK_NULL_HANDLE)==defaults);
    correct=correct&&((g->mpm_phase_pipelines[0]!=VK_NULL_HANDLE)==defaults)&&
        ((g->mpm_phase_pipelines[4]!=VK_NULL_HANDLE)==defaults);
    correct=correct&&((g->contact_boundary_pipeline==VK_NULL_HANDLE)==defaults);
    printf("profile defaults%d swap%d specialized%d fused%d overlap%d zero%d boundary-cache%d\n",
        defaults,g->mpm_swap_buffers,g->mpm_phase_pipelines[0]!=VK_NULL_HANDLE,g->mpm_fused_grid,
        g->solver_apply_overlap,g->solver_skip_zero_reductions,g->contact_boundary_pipeline!=VK_NULL_HANDLE);
    dc_gpu_destroy(g);ASSERT_TRUE(correct);PASS();
}
static void test_fresh_context_uses_the_measured_profile(void) {profile_case(true);}
static void test_explicit_reference_options_remain_available(void) {profile_case(false);}
int main(void) {
    RUN(test_fresh_context_uses_the_measured_profile);
    RUN(test_explicit_reference_options_remain_available);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
