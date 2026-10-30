#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compound_fixture.h"
static int g_pass,g_fail;
#define RUN(fn) do { printf("RUN  %s\n",#fn); fn(); printf("OK   %s\n",#fn); } while(0)
#define ASSERT_TRUE(e) do { if(!(e)) { printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#e); ++g_fail; return; } } while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() ++g_pass
static dc_gpu_t *grid(void) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *c=calloc(1,sizeof(*c));
    bool okay=c && dc_gpu_create(&g,128,128,"build/shaders/pattern.comp.spv",err,sizeof(err));
    for(uint32_t i=0;okay && i<4;++i)
        okay=dc_gpu_upload_chunk(g,i,c,err,sizeof(err)) && dc_gpu_set_page(g,i%2,i/2,i,err,sizeof(err));
    free(c);if(!okay) { dc_gpu_destroy(g);return NULL; }return g;
}
static void test_compounds_preserve_cavities_and_material_mass(void) {
    char err[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);
    dc_gpu_compound_shape_t shapes[2]={fixture_l(),fixture_u()};
    for(uint32_t i=0;i<2;++i)
        ASSERT_TRUE(dc_gpu_spawn_compound_body(g,fixture_body(i+1,20+40*i,20),&shapes[i],err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    for(uint32_t i=0;i<2;++i) {
        uint32_t current;
        ASSERT_TRUE(dc_gpu_read_occupancy(g,28+40*i,24,&current,NULL,err,sizeof(err)));ASSERT_EQ(current,0u);
        ASSERT_TRUE(dc_gpu_read_occupancy(g,21+40*i,24,&current,NULL,err,sizeof(err)));ASSERT_EQ(current,i+1);
        dc_gpu_body_motion_t m;ASSERT_TRUE(dc_gpu_read_body_motion(g,i+1,&m,err,sizeof(err)));
        ASSERT_TRUE(fabsf(m.mass-(i ? 160*.6f : 112*2.7f))<.001f);
        ASSERT_TRUE(fabsf(m.center_x-(i ? 8 : 38.f/7))<.001f);
        ASSERT_TRUE(fabsf(m.center_y-(i ? 9.2f : 74.f/7))<.001f);
    }
    dc_gpu_destroy(g);PASS();
}
static void test_cavity_excludes_terrain_and_body_contacts(void) {
    char err[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);dc_gpu_compound_shape_t u=fixture_u();
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g,fixture_body(2,60,20),&u,err,sizeof(err)));
    dc_gpu_world_body_t small=fixture_body(3,67,22);small.body.width=2;small.body.height=2;
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,small,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,68,26,0,DC_MATERIAL_STONE,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    dc_gpu_contact_stats_t stats;dc_gpu_contact_t contacts[32];
    ASSERT_TRUE(dc_gpu_read_contacts(g,&stats,contacts,32,err,sizeof(err)));ASSERT_EQ(stats.count,0u);
    ASSERT_TRUE(dc_gpu_paint_material(g,61,24,0,DC_MATERIAL_STONE,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_contacts(g,&stats,contacts,32,err,sizeof(err)));
    printf("compound wall contacts: %u\n",stats.count);
    ASSERT_EQ(stats.count,1u);ASSERT_EQ(contacts[0].body_a,2u);ASSERT_EQ(stats.overflow,0u);
    ASSERT_TRUE(dc_gpu_paint_material(g,73,24,0,DC_MATERIAL_STONE,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_contacts(g,&stats,contacts,32,err,sizeof(err)));
    ASSERT_EQ(stats.count,2u);
    uint32_t piece_mask=0;
    for(uint32_t i=0;i<stats.count;++i) piece_mask|=1u<<((contacts[i].feature_a>>28)&3u);
    ASSERT_EQ(piece_mask,3u);
    ASSERT_TRUE(dc_gpu_paint_material(g,73,24,0,DC_MATERIAL_WATER,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_contacts(g,&stats,contacts,32,err,sizeof(err)));
    ASSERT_EQ(stats.count,1u);
    ASSERT_EQ(contacts[0].body_a,2u);
    ASSERT_EQ(stats.overflow,0u);
    dc_gpu_destroy(g);PASS();
}
static void test_paused_terrain_brush_refreshes_compound_contacts(void) {
    char err[256] = {0};
    dc_gpu_t *g = grid();
    ASSERT_TRUE(g);
    dc_gpu_compound_shape_t u = fixture_u();
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g, fixture_body(2, 60, 20),
                                            &u, err, sizeof(err)) &&
                dc_gpu_set_body_origin(g, (dc_chunk_coord_t){0, 0},
                                       err, sizeof(err)));
    dc_gpu_contact_stats_t stats;
    dc_gpu_contact_t contacts[8];
    ASSERT_TRUE(dc_gpu_read_contacts(g, &stats, contacts, 8,
                                     err, sizeof(err)));
    ASSERT_EQ(stats.count, 0u);
    ASSERT_TRUE(dc_gpu_paint_material(g, 61, 24, 0, DC_MATERIAL_STONE,
                                      err, sizeof(err)) &&
                dc_gpu_read_contacts(g, &stats, contacts, 8,
                                     err, sizeof(err)));
    ASSERT_EQ(stats.count, 1u);
    ASSERT_EQ(contacts[0].body_a, 2u);
    ASSERT_TRUE(dc_gpu_paint_material(g, 61, 24, 0, DC_MATERIAL_WATER,
                                      err, sizeof(err)) &&
                dc_gpu_read_contacts(g, &stats, contacts, 8,
                                     err, sizeof(err)));
    ASSERT_EQ(stats.count, 0u);
    dc_gpu_destroy(g);
    PASS();
}
static void test_invalid_compounds_leave_existing_body_unchanged(void) {
    char err[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);dc_gpu_compound_shape_t l=fixture_l(),bad=l,after;
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g,fixture_body(1,20,20),&l,err,sizeof(err)));
    bad.pieces[1]=bad.pieces[0];
    ASSERT_TRUE(!dc_gpu_spawn_compound_body(g,fixture_body(1,40,40),&bad,err,sizeof(err)));
    bad=l;bad.pieces[1]=fixture_rectangle(DC_GPU_BODY_STONE,8,12,8,4);
    ASSERT_TRUE(!dc_gpu_spawn_compound_body(g,fixture_body(1,40,40),&bad,err,sizeof(err)));
    bad=l;bad.pieces[1].material=99u;
    ASSERT_TRUE(!dc_gpu_spawn_compound_body(g,fixture_body(1,40,40),&bad,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_compound_shape(g,1,&after,err,sizeof(err)));ASSERT_EQ(memcmp(&l,&after,sizeof(l)),0);
    dc_gpu_world_body_t body;ASSERT_TRUE(dc_gpu_read_world_body(g,1,&body,err,sizeof(err)));
    ASSERT_EQ(body.body.x_fp,20<<16);dc_gpu_destroy(g);PASS();
}
static void test_compound_snapshot_preserves_pieces_and_angular_state(void) {
    char err[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);dc_gpu_compound_shape_t u=fixture_u(),after;
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g,fixture_body(2,60,20),&u,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,2,.3f,.1f,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_motion_t before,motion;ASSERT_TRUE(dc_gpu_read_body_motion(g,2,&before,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_save_bodies(g,"build/compound_snapshot.bin",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_remove_body(g,2,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_load_bodies(g,"build/compound_snapshot.bin",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_compound_shape(g,2,&after,err,sizeof(err)));ASSERT_EQ(memcmp(&u,&after,sizeof(u)),0);
    ASSERT_TRUE(dc_gpu_read_body_motion(g,2,&motion,err,sizeof(err)));ASSERT_EQ(memcmp(&before,&motion,sizeof(motion)),0);
    remove("build/compound_snapshot.bin");dc_gpu_destroy(g);PASS();
}
static void test_supported_compounds_stop_changing_pose(void) {
    char err[256];dc_gpu_t *g=grid();ASSERT_TRUE(g);
    for(uint32_t x=0;x<128;++x)
        ASSERT_TRUE(dc_gpu_paint_material(g,x,80,0,DC_MATERIAL_STONE,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    dc_gpu_compound_shape_t l=fixture_l(),u=fixture_u();
    dc_gpu_world_body_t a=fixture_body(1,50,20),b=fixture_body(2,59,45);
    a.body.vx_fp=32768;
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g,a,&l,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g,b,&u,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,1,0,.12f,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,2,0,-.06f,0,err,sizeof(err)));
    for(uint32_t i=0;i<360;++i) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_world_body_t before[2],after;dc_gpu_body_motion_t motions[2],motion;
    for(uint32_t id=1;id<=2;++id) {
        ASSERT_TRUE(dc_gpu_read_world_body(g,id,&before[id-1],err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(g,id,&motions[id-1],err,sizeof(err)));
    }
    for(uint32_t i=0;i<60;++i) {
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
        for(uint32_t id=1;id<=2;++id) {
            ASSERT_TRUE(dc_gpu_read_world_body(g,id,&after,err,sizeof(err)));
            ASSERT_TRUE(dc_gpu_read_body_motion(g,id,&motion,err,sizeof(err)));
            ASSERT_EQ(memcmp(&before[id-1],&after,sizeof(after)),0);
            ASSERT_EQ(memcmp(&motions[id-1],&motion,sizeof(motion)),0);
        }
    }
    for(uint32_t x=0;x<128;++x)
        ASSERT_TRUE(dc_gpu_paint_material(g,x,80,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g,1,&after,err,sizeof(err)));
    double old_y=before[0].chunk.y*64+before[0].body.y_fp/65536.0;
    double new_y=after.chunk.y*64+after.body.y_fp/65536.0;
    ASSERT_TRUE(new_y>old_y+.2 && after.body.vy_fp>13107);
    dc_gpu_destroy(g);PASS();
}
static void test_slow_kinematic_support_does_not_freeze_passenger(void) {
    char err[256];dc_gpu_t *g=grid();ASSERT_TRUE(g && dc_gpu_set_rigid_solver(g,true));
    dc_gpu_world_body_t support=fixture_body(1,20,60),passenger=fixture_body(2,24,56);
    support.body.height=4;support.body.vx_fp=655;
    passenger.body.width=4;passenger.body.height=4;
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,support,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,1,0,0,DC_GPU_BODY_KINEMATIC|DC_GPU_BODY_LOCK_ROTATION,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,passenger,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,2,0,0,DC_GPU_BODY_LOCK_ROTATION,err,sizeof(err)));
    for(uint32_t i=0;i<120;++i) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g,2,&passenger,err,sizeof(err)));
    ASSERT_TRUE(passenger.body.x_fp>24*65536+65536/2);
    ASSERT_TRUE(fabs(passenger.body.y_fp/65536.0-56)<.1);
    dc_gpu_destroy(g);PASS();
}
static void test_stationary_kinematic_support_stops_passenger_drift(void) {
    char err[256];dc_gpu_t *g=grid();ASSERT_TRUE(g && dc_gpu_set_rigid_solver(g,true));
    dc_gpu_world_body_t support=fixture_body(1,20,60),passenger=fixture_body(2,20,44),before;
    support.body.height=4;
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,support,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,1,0,0,DC_GPU_BODY_KINEMATIC|DC_GPU_BODY_LOCK_ROTATION,err,sizeof(err)));
    dc_gpu_compound_shape_t l=fixture_l();
    ASSERT_TRUE(dc_gpu_spawn_compound_body(g,passenger,&l,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,2,0,0,0,err,sizeof(err)));
    for(uint32_t i=0;i<120;++i) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g,2,&before,err,sizeof(err)));
    ASSERT_EQ(before.body.vx_fp,0);ASSERT_EQ(before.body.vy_fp,0);
    for(uint32_t i=0;i<30;++i) {
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_world_body(g,2,&passenger,err,sizeof(err)));
        ASSERT_EQ(memcmp(&before,&passenger,sizeof(before)),0);
    }
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){-1,0},err,sizeof(err)));
    for(uint32_t i=0;i<10;++i) {
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_world_body(g,2,&passenger,err,sizeof(err)));
        ASSERT_EQ(memcmp(&before,&passenger,sizeof(before)),0);
    }
    dc_gpu_destroy(g);PASS();
}
int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--paused-brush") == 0) {
        RUN(test_paused_terrain_brush_refreshes_compound_contacts);
        return g_fail ? 1 : 0;
    }
    RUN(test_stationary_kinematic_support_stops_passenger_drift);
    RUN(test_supported_compounds_stop_changing_pose);
    RUN(test_slow_kinematic_support_does_not_freeze_passenger);
    RUN(test_compounds_preserve_cavities_and_material_mass);
    RUN(test_cavity_excludes_terrain_and_body_contacts);
    RUN(test_paused_terrain_brush_refreshes_compound_contacts);
    RUN(test_invalid_compounds_leave_existing_body_unchanged);
    RUN(test_compound_snapshot_preserves_pieces_and_angular_state);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail ? 1 : 0;
}
