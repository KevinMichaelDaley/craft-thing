#include <stdio.h>
#include <stdlib.h>

#include "../../src/vulkan/gpu_internal.h"
#include "../../src/vulkan/renderer_internal.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void support_case(bool held) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    unsigned source=20*64+30;
    for(unsigned y=18;y<40;y++) chunk->cells[y*64+29]=
        (dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    chunk->cells[source]=(dc_cell_t){.material=DC_MATERIAL_DIRT,.flags=held?DC_CELL_LOADED_TERRAIN:0};
    dc_chunk_particle_init(chunk->particles+source,chunk->coord,source,DC_MATERIAL_DIRT);
    chunk->particles[source].vy_fp=65536;chunk->particle_count=1;
    uint64_t initial_mass=chunk->particles[source].mass_fp;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_gpu_structural_data(g)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD]=0u;
    for(unsigned tick=0;tick<12;tick++) {
        ASSERT_TRUE(fr_renderer_begin(g));
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,0,1,&g->descriptor,0,NULL);
        dc_gpu_record_mpm(g);
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
        ASSERT_TRUE(fr_renderer_submit(g));
    }
    uint32_t dirty=dc_gpu_structural_data(g)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD];
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    uint64_t mass=0;for(unsigned i=0;i<DC_MPM_PARTICLES_PER_CHUNK;i++) mass+=chunk->particles[i].mass_fp;
    printf("held%d: source material%u, mass%llu, dirty%u\n",held,chunk->cells[source].material,
        (unsigned long long)mass,dirty);
    ASSERT_EQ(chunk->cells[source].material,DC_MATERIAL_AIR);ASSERT_EQ(mass,initial_mass);
    ASSERT_EQ(dirty,held?1u:0u);
    dc_gpu_destroy(g);free(chunk);PASS();
}
static void test_loose_powder_motion_does_not_invalidate_terrain_connectivity(void) {support_case(false);}
static void test_vacated_held_grain_still_invalidates_terrain_connectivity(void) {support_case(true);}
int main(void) {
    RUN(test_loose_powder_motion_does_not_invalidate_terrain_connectivity);
    RUN(test_vacated_held_grain_still_invalidates_terrain_connectivity);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
