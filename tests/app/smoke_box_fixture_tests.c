#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../src/app/smoke_box.h"
static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s [%s]\n",__LINE__,#e,err);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++
static void test_fixture_has_stone_enclosure_necks_vent_and_full_density_anchors(void) {
    char err[256]={0};dc_chunk_coord_t coords[]={{1,0},{2,1},{5,1},{6,-1}};dc_smoke_box_chunk_t chunks[4]={0};
    ASSERT_TRUE(dc_smoke_box_build(coords,4,chunks,err,sizeof(err)));
    ASSERT_EQ(chunks[0].chunk->cells[0*64+16].material,DC_MATERIAL_STONE);
    ASSERT_EQ(chunks[0].chunk->cells[47*64+60].material,DC_MATERIAL_AIR);
    ASSERT_EQ(chunks[1].chunk->cells[32*64].material,DC_MATERIAL_STONE);
    ASSERT_TRUE(chunks[1].chunk->cells[32*64].flags&DC_CELL_LOADED_TERRAIN);
    ASSERT_EQ(chunks[3].chunk->cells[24*64+15].material,DC_MATERIAL_AIR);
    for(uint32_t k=0;k<4;k++) for(uint32_t i=0;i<4096;i++) {
        ASSERT_EQ(chunks[k].void_fractions[i],chunks[k].chunk->cells[i].material==DC_MATERIAL_AIR?1.:0.);
        ASSERT_EQ(chunks[k].chunk->cells[i].fluid_mass,0u);
    }
    dc_smoke_box_release(chunks,4);PASS();
}
static void test_finite_colored_reservoirs_have_equal_ambient_pressure_lower_density_and_no_oxygen(void) {
    char err[256]={0};dc_chunk_coord_t coords[]={{2,1},{5,1},{3,0}};dc_smoke_box_chunk_t chunks[3]={0};
    ASSERT_TRUE(dc_smoke_box_build(coords,3,chunks,err,sizeof(err)));fr_gas_model_t m=dc_smoke_box_model();
    fr_gas_state_t red=chunks[0].gas[8*64+8],blue=chunks[1].gas[8*64+8],air=chunks[2].gas[0];
    fr_gas_thermo_t tr,tb,ta;
    ASSERT_TRUE(fr_gas_thermodynamics(&m,&red,1,&tr,err,sizeof(err))&&fr_gas_thermodynamics(&m,&blue,1,&tb,err,sizeof(err))&&fr_gas_thermodynamics(&m,&air,1,&ta,err,sizeof(err)));
    ASSERT_TRUE(fabs(tr.temperature_K-650.)<1e-4&&fabs(tb.temperature_K-650.)<1e-4);
    ASSERT_TRUE(fabs(tr.pressure/ta.pressure-1.)<2e-5&&fabs(tb.pressure/ta.pressure-1.)<2e-5);
    ASSERT_TRUE(tr.mass<ta.mass&&tr.mass>0&&tr.oxygen_mole_fraction==0&&tb.oxygen_mole_fraction==0);
    ASSERT_TRUE(fabs(ta.oxygen_mole_fraction-.21)<1e-6);
    ASSERT_EQ(memcmp(red.species,blue.species,sizeof(red.species)),0);ASSERT_EQ(red.energy,blue.energy);
    ASSERT_TRUE(red.pigment[0]>red.pigment[2]&&blue.pigment[2]>blue.pigment[0]);
    double stock=0;for(uint32_t k=0;k<2;k++) for(uint32_t i=0;i<4096;i++) stock+=chunks[k].gas[i].species[2];
    ASSERT_TRUE(stock>0&&stock<4.);dc_smoke_box_release(chunks,3);PASS();
}
int main(void) {
    RUN(test_fixture_has_stone_enclosure_necks_vent_and_full_density_anchors);
    RUN(test_finite_colored_reservoirs_have_equal_ambient_pressure_lower_density_and_no_oxygen);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
