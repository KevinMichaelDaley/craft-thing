#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../src/app/bonfire.h"
#include "ferrum/fire/material_profile.h"
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
        ASSERT_TRUE(wood>=300);
    }
    dc_thermal_bench_release(chunks,2);PASS();
}
static void test_cold_logs_wet_control_and_finite_starter_retain_real_state(void) {
    char err[256]={0};dc_chunk_coord_t coords[2]={{2,1},{5,1}};dc_bonfire_plan_t plan;ASSERT_TRUE(dc_bonfire_plan(&plan));
    dc_thermal_bench_chunk_t *chunks=calloc(2,sizeof(*chunks));ASSERT_TRUE(chunks&&dc_bonfire_build(coords,2,chunks,err,sizeof(err)));
    uint32_t hot[2]={0},wet[2]={0};double ember_heat[2]={0},ember_fuel[2]={0};
    for(uint32_t side=0;side<2;side++) for(uint32_t j=0;j<chunks[side].reactive.count;j++) {
        fr_reactive_attachment_t *a=chunks[side].reactive.attachments+j;fr_reactive_state_t *s=&a->record.state;
        if(s->material!=DC_MATERIAL_WOOD) continue;
        uint32_t x=(uint32_t)coords[side].x*64+a->local_index%64,y=64+a->local_index/64;const dc_bonfire_piece_t *piece=NULL;
        for(uint32_t k=0;k<plan.count;k++) if(x>=plan.pieces[k].x&&x<plan.pieces[k].x+plan.pieces[k].width&&y>=plan.pieces[k].y&&y<plan.pieces[k].y+plan.pieces[k].height) piece=plan.pieces+k;
        ASSERT_TRUE(piece);float temperature=0;
        ASSERT_TRUE(fr_reactive_state_temperature(s,&temperature,err,sizeof(err))&&fabsf(temperature-piece->temperature_K)<.001f);
        if(piece->kind==DC_BONFIRE_STARTER) {hot[side]++;ASSERT_EQ(s->bound_water_fp,0);
            ASSERT_EQ(s->fuel_mass_fp,0);ASSERT_TRUE(s->char_mass_fp>0&&(s->flags&FR_REACTIVE_FLAMING));
            ASSERT_TRUE(temperature>=1700&&temperature<=1900);
            ember_heat[side]+=fr_reactive_state_energy(s);
            ember_fuel[side]+=(double)s->char_mass_fp/65536.*fr_reactive_profile(DC_MATERIAL_CHAR)->chemical_energy;}
        else {ASSERT_TRUE(s->fuel_mass_fp>0&&!(s->flags&FR_REACTIVE_FLAMING));
            if(side) {ASSERT_TRUE(s->bound_water_fp>0);wet[side]++;}
            else ASSERT_EQ(s->bound_water_fp,0);}
    }
    ASSERT_EQ(hot[0],8);ASSERT_EQ(hot[1],8);ASSERT_TRUE(wet[1]>=300);
    for(uint32_t side=0;side<2;side++) {
        ASSERT_TRUE(ember_heat[side]>8000&&ember_heat[side]<9000);
        ASSERT_TRUE(ember_fuel[side]>130000&&ember_fuel[side]<140000);
    }
    dc_thermal_bench_release(chunks,2);PASS();
}
static bool bonfire_rect_contact(uint32_t a,uint32_t b) {
    const uint32_t *r=dc_bonfire_rects[a],*s=dc_bonfire_rects[b];
    bool horizontal=(r[0]+r[2]==s[0]||s[0]+s[2]==r[0])&&
        r[1]<s[1]+s[3]&&s[1]<r[1]+r[3];
    bool vertical=(r[1]+r[3]==s[1]||s[1]+s[3]==r[1])&&
        r[0]<s[0]+s[2]&&s[0]<r[0]+r[2];
    return horizontal||vertical;
}
static void test_right_starter_contacts_all_three_right_logs_without_extra_wood(void) {
    char err[1]={0};
    uint32_t starter_area=0;
    for(uint32_t i=0;i<DC_BONFIRE_KINDLING_COUNT;i++) starter_area+=dc_bonfire_rects[i][2]*dc_bonfire_rects[i][3];
    ASSERT_EQ(starter_area,48u);
    const uint32_t chain[]={12u,0u,4u,5u,11u,15u,10u,17u,9u,19u,20u};
    for(uint32_t i=1;i<sizeof(chain)/sizeof(chain[0]);i++) ASSERT_TRUE(bonfire_rect_contact(chain[i-1],chain[i]));
    PASS();
}
static void test_left_starter_contacts_nearest_dry_log_without_extra_wood(void) {
    char err[1]={0};
    const uint32_t chain[]={12u,6u,13u,7u,16u};
    for(uint32_t i=1;i<sizeof(chain)/sizeof(chain[0]);i++) ASSERT_TRUE(bonfire_rect_contact(chain[i-1],chain[i]));
    PASS();
}
static void test_every_log_has_a_nonoverlapping_face_path_to_the_starter(void) {
    char err[1]={0};bool reached[DC_BONFIRE_PIECE_COUNT]={0};reached[12]=true;
    for(uint32_t pass=0;pass<DC_BONFIRE_PIECE_COUNT;pass++)
        for(uint32_t i=0;i<DC_BONFIRE_PIECE_COUNT;i++) if(reached[i])
            for(uint32_t j=0;j<DC_BONFIRE_PIECE_COUNT;j++)
                if(bonfire_rect_contact(i,j)) reached[j]=true;
    for(uint32_t i=DC_BONFIRE_LOG_FIRST;i<DC_BONFIRE_PIECE_COUNT;i++) ASSERT_TRUE(reached[i]);
    for(uint32_t i=0;i<DC_BONFIRE_PIECE_COUNT;i++) for(uint32_t j=i+1;j<DC_BONFIRE_PIECE_COUNT;j++) {
        const uint32_t *a=dc_bonfire_rects[i],*b=dc_bonfire_rects[j];
        ASSERT_TRUE(a[0]+a[2]<=b[0]||b[0]+b[2]<=a[0]||a[1]+a[3]<=b[1]||b[1]+b[3]<=a[1]);
    }
    PASS();
}
static void test_chunk_reservations_advance_with_fixture_import_order(void) {
    char err[256]={0};dc_chunk_coord_t coords[2]={{2,1},{5,1}};
    dc_thermal_bench_chunk_t *chunks=calloc(2,sizeof(*chunks));ASSERT_TRUE(chunks);
    ASSERT_TRUE(dc_bonfire_build(coords,2,chunks,err,sizeof(err)));
    ASSERT_EQ(chunks[0].reactive.next_id,4097u);
    ASSERT_EQ(chunks[1].reactive.next_id,8193u);
    dc_thermal_bench_release(chunks,2);PASS();
}
int main(void) {RUN(test_only_ground_is_terrain_and_all_fuel_is_rigid_staging);RUN(test_cold_logs_wet_control_and_finite_starter_retain_real_state);RUN(test_right_starter_contacts_all_three_right_logs_without_extra_wood);RUN(test_left_starter_contacts_nearest_dry_log_without_extra_wood);RUN(test_every_log_has_a_nonoverlapping_face_path_to_the_starter);
    RUN(test_chunk_reservations_advance_with_fixture_import_order);
    printf("Results: %d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;}
