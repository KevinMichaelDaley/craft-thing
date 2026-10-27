#include <stdio.h>
#include <stdlib.h>
#include "../../src/app/bonfire.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_dry_and_wet_piles_have_real_fuel_loaded_flags_and_colors(void) {
    char err[256]={0};dc_chunk_coord_t coords[2]={{2,1},{5,1}};
    dc_thermal_bench_chunk_t *chunks=calloc(2,sizeof(*chunks));ASSERT_TRUE(chunks);
    ASSERT_TRUE(dc_bonfire_build(coords,2,chunks,err,sizeof(err)));
    uint32_t wood[2]={0},wet[2]={0},stone[2]={0};
    for(uint32_t side=0;side<2;side++) for(uint32_t i=0;i<4096;i++) {
        dc_cell_t cell=chunks[side].chunk->cells[i];if(!cell.material) continue;
        ASSERT_TRUE((cell.flags&DC_CELL_LOADED_TERRAIN)!=0);
        ASSERT_TRUE(chunks[side].colors->terrain[i][3]>0);
        if(cell.material==DC_MATERIAL_WOOD) wood[side]++;
        if(cell.material==DC_MATERIAL_STONE) stone[side]++;
    }
    for(uint32_t side=0;side<2;side++) for(uint32_t i=0;i<chunks[side].reactive.count;i++) {
        fr_reactive_state_t state=chunks[side].reactive.attachments[i].record.state;
        if(state.material==DC_MATERIAL_WOOD) {
            ASSERT_TRUE(state.fuel_mass_fp>0);if(state.bound_water_fp>0) wet[side]++;
        }
    }
    ASSERT_TRUE(wood[0]>0&&wood[1]>0&&stone[0]>0&&stone[1]>0);
    ASSERT_EQ(wet[0],0);ASSERT_EQ(wet[1],wood[1]);
    dc_thermal_bench_release(chunks,2);PASS();
}
static void test_kindling_has_airflow_and_one_finite_ember_but_no_lit_logs(void) {
    char err[256]={0};dc_chunk_coord_t coord={2,1};
    dc_thermal_bench_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_bonfire_build(&coord,1,chunk,err,sizeof(err)));
    uint32_t embers=0,kindling_faces=0;double starting_energy=0;
    for(uint32_t i=0;i<chunk->reactive.count;i++) {
        fr_reactive_attachment_t *a=chunk->reactive.attachments+i;fr_reactive_state_t *s=&a->record.state;
        float temperature=0;ASSERT_TRUE(fr_reactive_state_temperature(s,&temperature,err,sizeof(err)));
        ASSERT_TRUE(!(s->flags&FR_REACTIVE_FLAMING));
        if(s->material==DC_MATERIAL_WOOD) ASSERT_TRUE(temperature<294);
        if(s->material==DC_MATERIAL_CHAR) {
            embers++;ASSERT_TRUE(s->fuel_mass_fp>0&&temperature>1000&&temperature<1600);
            starting_energy+=s->thermal_energy;
        }
    }
    for(uint32_t y=26;y<28;y++) for(uint32_t x=24;x<40;x++) {
        uint32_t at=y*64+x;
        if(chunk->chunk->cells[at].material==DC_MATERIAL_WOOD&&
           (chunk->chunk->cells[at-1].material==DC_MATERIAL_AIR||chunk->chunk->cells[at+1].material==DC_MATERIAL_AIR)) kindling_faces++;
    }
    ASSERT_EQ(embers,1);ASSERT_TRUE(starting_energy>0&&starting_energy<1000&&kindling_faces>=4);
    dc_thermal_bench_release(chunk,1);PASS();
}
int main(void) {RUN(test_dry_and_wet_piles_have_real_fuel_loaded_flags_and_colors);
    RUN(test_kindling_has_airflow_and_one_finite_ember_but_no_lit_logs);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
