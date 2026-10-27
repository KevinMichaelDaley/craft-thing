#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../src/app/bonfire.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_only_ground_is_terrain_and_all_fuel_is_rigid_staging(void) {
    char err[256]={0};dc_chunk_coord_t coords[2]={{2,1},{5,1}};
    dc_thermal_bench_chunk_t *chunks=calloc(2,sizeof(*chunks));ASSERT_TRUE(chunks);
    ASSERT_TRUE(dc_bonfire_build(coords,2,chunks,err,sizeof(err)));
    for(uint32_t side=0;side<2;side++) {
        uint32_t wood=0;
        for(uint32_t i=0;i<4096;i++) {
            dc_cell_t cell=chunks[side].chunk->cells[i];uint32_t y=64+i/64;
            if(!cell.material) continue;
            ASSERT_TRUE(chunks[side].colors->terrain[i][3]>0);
            if(y>=96) {ASSERT_EQ(cell.material,DC_MATERIAL_STONE);ASSERT_TRUE(cell.flags&DC_CELL_LOADED_TERRAIN);}
            else {ASSERT_EQ(cell.material,DC_MATERIAL_WOOD);ASSERT_EQ(cell.flags,0);wood++;}
        }
        ASSERT_EQ(wood,123);
    }
    dc_thermal_bench_release(chunks,2);PASS();
}
static void test_cold_logs_wet_control_and_finite_starter_retain_real_state(void) {
    char err[256]={0};dc_chunk_coord_t coords[2]={{2,1},{5,1}};dc_bonfire_plan_t plan;ASSERT_TRUE(dc_bonfire_plan(&plan));
    dc_thermal_bench_chunk_t *chunks=calloc(2,sizeof(*chunks));ASSERT_TRUE(chunks&&dc_bonfire_build(coords,2,chunks,err,sizeof(err)));
    uint32_t hot[2]={0},wet[2]={0};
    for(uint32_t side=0;side<2;side++) for(uint32_t j=0;j<chunks[side].reactive.count;j++) {
        fr_reactive_attachment_t *a=chunks[side].reactive.attachments+j;fr_reactive_state_t *s=&a->record.state;
        if(s->material!=DC_MATERIAL_WOOD) continue;
        uint32_t x=(uint32_t)coords[side].x*64+a->local_index%64,y=64+a->local_index/64;const dc_bonfire_piece_t *piece=NULL;
        for(uint32_t k=0;k<plan.count;k++) if(x>=plan.pieces[k].x&&x<plan.pieces[k].x+plan.pieces[k].width&&y>=plan.pieces[k].y&&y<plan.pieces[k].y+plan.pieces[k].height) piece=plan.pieces+k;
        ASSERT_TRUE(piece);float temperature=0;
        ASSERT_TRUE(fr_reactive_state_temperature(s,&temperature,err,sizeof(err))&&fabsf(temperature-piece->temperature_K)<.001f);
        if(piece->kind==DC_BONFIRE_STARTER) {hot[side]++;ASSERT_EQ(s->bound_water_fp,0);
            ASSERT_EQ(s->fuel_mass_fp,0);ASSERT_TRUE(s->char_mass_fp>0&&(s->flags&FR_REACTIVE_FLAMING));}
        else {ASSERT_TRUE(s->fuel_mass_fp>0&&!(s->flags&FR_REACTIVE_FLAMING));
            if(side) {ASSERT_TRUE(s->bound_water_fp>0);wet[side]++;}
            else ASSERT_EQ(s->bound_water_fp,0);}
    }
    ASSERT_EQ(hot[0],3);ASSERT_EQ(hot[1],3);ASSERT_EQ(wet[1],120);
    dc_thermal_bench_release(chunks,2);PASS();
}
int main(void) {RUN(test_only_ground_is_terrain_and_all_fuel_is_rigid_staging);RUN(test_cold_logs_wet_control_and_finite_starter_retain_real_state);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
