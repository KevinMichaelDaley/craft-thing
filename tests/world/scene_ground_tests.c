#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/generate.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_EQ((int)(a),(int)(b))
#define PASS() g_pass++

static void test_scene_soil_surface_is_not_buried_under_procedural_stone(void) {
    uint32_t surface[128];
    for(uint32_t i=0;i<128;i++) surface[i]=i<64?76:67;
    dc_ground_profile_t ground={.origin_x=-32,.origin_y=-32,.width=128,.soil_depth=12,.surface=surface};
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_generate_chunk_ground((dc_chunk_coord_t){-1,0},&ground,chunk));
    ASSERT_EQ(chunk->cells[43*64+32].material,DC_MATERIAL_AIR);
    ASSERT_EQ(chunk->cells[44*64+32].material,DC_MATERIAL_DIRT);
    ASSERT_EQ(chunk->cells[55*64+32].material,DC_MATERIAL_DIRT);
    ASSERT_EQ(chunk->cells[56*64+32].material,DC_MATERIAL_STONE);
    ASSERT_TRUE(chunk->particle_count>0);
    ASSERT_TRUE(dc_generate_chunk_ground((dc_chunk_coord_t){1,0},&ground,chunk));
    ASSERT_EQ(chunk->cells[34*64+31].material,DC_MATERIAL_AIR);
    ASSERT_EQ(chunk->cells[35*64+31].material,DC_MATERIAL_DIRT);
    free(chunk);PASS();
}
static void test_neighboring_chunks_extend_the_authored_edge_heights(void) {
    uint32_t surface[2]={76,67};
    dc_ground_profile_t ground={.origin_x=-32,.origin_y=-32,.width=2,.soil_depth=12,.surface=surface};
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_generate_chunk_ground((dc_chunk_coord_t){-10,0},&ground,chunk));
    ASSERT_EQ(chunk->cells[43*64].material,DC_MATERIAL_AIR);
    ASSERT_EQ(chunk->cells[44*64].material,DC_MATERIAL_DIRT);
    ASSERT_TRUE(dc_generate_chunk_ground((dc_chunk_coord_t){10,0},&ground,chunk));
    ASSERT_EQ(chunk->cells[34*64].material,DC_MATERIAL_AIR);
    ASSERT_EQ(chunk->cells[35*64].material,DC_MATERIAL_DIRT);
    free(chunk);PASS();
}
int main(void) {
    RUN(test_scene_soil_surface_is_not_buried_under_procedural_stone);
    RUN(test_neighboring_chunks_extend_the_authored_edge_heights);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
