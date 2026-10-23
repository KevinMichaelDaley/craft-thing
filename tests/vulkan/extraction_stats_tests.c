#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/vulkan/gpu_internal.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
VkResult __wrap_vkQueueWaitIdle(VkQueue queue) {(void)queue;return VK_SUCCESS;}
static void diagnostic_case(unsigned kind) {
    dc_gpu_t gpu={.width=64,.height=64,.automatic_components_enabled=true};
    gpu.body_mapped=calloc(1,dc_gpu_body_storage_bytes(&gpu));ASSERT_TRUE(gpu.body_mapped);
    uint32_t *base=dc_gpu_structural_data(&gpu),*graph=base+dc_gpu_structural_graph_offset();
    base[DC_STRUCTURAL_COMPONENT_DIRTY_WORD]=1;graph[36257]=1;
    uint32_t *record=graph+36300,*pool=graph+40000;
    pool[2]=100;record[7]=1;record[10]=41;
    dc_gpu_body_record_t *bodies=gpu.body_mapped;
    if(kind==0) for(unsigned slot=0;slot<64;slot++) bodies[slot].body.active=1;
    if(kind==1) {record[6]=1;record[7]=0;record[10]=0;}
    if(kind==2) {pool[2]=40;}
    dc_gpu_extraction_stats_t stats={0};
    bool okay=dc_gpu_read_extraction_stats(&gpu,&stats);free(gpu.body_mapped);
    ASSERT_TRUE(okay);ASSERT_EQ(stats.body_capacity,64u);ASSERT_EQ(stats.pending,1u);
    ASSERT_EQ(stats.selected_components,1u);ASSERT_EQ(stats.committed_components,0u);
    ASSERT_EQ(stats.owned_bodies,kind==0?64u:0u);
    ASSERT_EQ(stats.unknown_components,kind==1?1u:0u);
    ASSERT_EQ(stats.no_body_slot_components,kind==0?1u:0u);
    ASSERT_EQ(stats.no_arena_components,kind==2?1u:0u);PASS();
}
static void test_full_body_pool_is_not_misreported_as_unknown_water_physics(void) {diagnostic_case(0);}
static void test_missing_world_connectivity_is_reported_separately(void) {diagnostic_case(1);}
static void test_geometry_arena_exhaustion_is_reported_separately(void) {diagnostic_case(2);}
int main(void) {
    RUN(test_full_body_pool_is_not_misreported_as_unknown_water_physics);
    RUN(test_missing_world_connectivity_is_reported_separately);
    RUN(test_geometry_arena_exhaustion_is_reported_separately);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
