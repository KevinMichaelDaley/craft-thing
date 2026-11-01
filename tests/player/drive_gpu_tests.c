#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/gpu.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/player/beaver.h"
#include "ferrum/player/drive_gpu.h"
#include "../../src/vulkan/runtime_body_gpu.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static dc_gpu_t *g_gpu;
static char g_err[256];
static fr_player_body_binding_t g_bindings[2];

static bool fixture(void) {
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    if (!chunk) return false;
    for (uint32_t x=0;x<64;x++) chunk->cells[40*64+x].material=DC_MATERIAL_STONE;
    bool okay=dc_gpu_create(&g_gpu,64,64,"build/shaders/pattern.comp.spv",g_err,sizeof(g_err)) &&
        dc_gpu_upload_chunk(g_gpu,0,chunk,g_err,sizeof(g_err)) &&
        dc_gpu_set_page(g_gpu,0,0,0,g_err,sizeof(g_err)) && dc_gpu_set_rigid_solver(g_gpu,true);
    free(chunk);
    for (uint32_t i=0;i<2 && okay;i++) {
        dc_gpu_world_body_t b={.body={.id=101+i,.active=1,.width=4,.height=4,
            .x_fp=(int32_t)(10+i*25)*65536,.y_fp=(int32_t)(i?10:36)*65536}};
        okay=dc_gpu_spawn_world_body(g_gpu,b,g_err,sizeof(g_err)) &&
            dc_gpu_set_body_motion(g_gpu,101+i,0,0,DC_GPU_BODY_LOCK_ROTATION,g_err,sizeof(g_err));
    }
    okay=okay && dc_gpu_set_body_origin(g_gpu,(dc_chunk_coord_t){0,0},g_err,sizeof(g_err)) &&
        dc_gpu_rigid_step(g_gpu,g_err,sizeof(g_err));
    for (uint32_t i=0;i<2 && okay;i++) {
        g_bindings[i]=(fr_player_body_binding_t){.identity=41+i,.body_id=101+i};
        for (uint32_t slot=0;slot<dc_gpu_body_capacity(g_gpu);slot++) {
            dc_body_storage_owner_t *owner=dc_gpu_body_owner(g_gpu,slot);
            if (owner->runtime_id==101+i && owner->state==DC_BODY_OWNER_LIVE)
                g_bindings[i].generation=owner->generation;
        }
        okay=g_bindings[i].generation!=0;
    }
    return okay;
}

static void test_real_body_drive_preserves_position_and_other_player(void) {
    fr_player_drive_gpu_t *drive=NULL;
    ASSERT_TRUE(fr_player_drive_gpu_create(g_gpu,g_bindings,2,1,2,&drive,g_err,sizeof(g_err)));
    dc_gpu_world_body_t before,after,other;
    ASSERT_TRUE(dc_gpu_read_world_body(g_gpu,101,&before,g_err,sizeof(g_err)));
    fr_player_drive_input_t input[2]={{41,1,1,false},{42,1,0,false}};
    ASSERT_TRUE(fr_player_drive_gpu_submit(drive,input,2,g_err,sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g_gpu,101,&after,g_err,sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g_gpu,102,&other,g_err,sizeof(g_err)));
    ASSERT_EQ(after.body.vx_fp,65536); ASSERT_EQ(other.body.vx_fp,0);
    ASSERT_EQ(after.body.x_fp,before.body.x_fp);ASSERT_EQ(after.body.y_fp,before.body.y_fp);
    ASSERT_EQ(after.body.vy_fp,before.body.vy_fp);
    ASSERT_TRUE(dc_gpu_rigid_step(g_gpu,g_err,sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g_gpu,101,&after,g_err,sizeof(g_err)));
    ASSERT_TRUE(after.body.x_fp>before.body.x_fp);
    fr_player_drive_gpu_destroy(drive);PASS();
}

static void test_jump_uses_actual_solver_support_and_consumes_support(void) {
    fr_player_drive_gpu_t *drive=NULL;
    ASSERT_TRUE(fr_player_drive_gpu_create(g_gpu,g_bindings,2,1,2,&drive,g_err,sizeof(g_err)));
    fr_player_drive_input_t input[2]={{41,1,0,true},{42,1,0,true}};
    ASSERT_TRUE(fr_player_drive_gpu_submit(drive,input,2,g_err,sizeof(g_err)));
    dc_gpu_world_body_t grounded,airborne;
    ASSERT_TRUE(dc_gpu_read_world_body(g_gpu,101,&grounded,g_err,sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g_gpu,102,&airborne,g_err,sizeof(g_err)));
    ASSERT_EQ(grounded.body.vy_fp,-2*65536); ASSERT_TRUE(airborne.body.vy_fp>=0);
    input[0].sequence=2;
    ASSERT_TRUE(fr_player_drive_gpu_submit(drive,input,2,g_err,sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g_gpu,g_err,sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g_gpu,101,&grounded,g_err,sizeof(g_err)));
    ASSERT_TRUE(grounded.body.y_fp<36*65536);
    fr_player_drive_gpu_destroy(drive);PASS();
}

static void test_sequence_retries_and_invalid_batches_are_transactional(void) {
    fr_player_drive_gpu_t *drive=NULL;
    ASSERT_TRUE(fr_player_drive_gpu_create(g_gpu,g_bindings,2,1,2,&drive,g_err,sizeof(g_err)));
    fr_player_drive_input_t input[2]={{41,UINT64_MAX-1,1,false},{42,1,-1,false}};
    ASSERT_TRUE(fr_player_drive_gpu_submit(drive,input,2,g_err,sizeof(g_err)));
    input[0].move_x=-1; input[1].move_x=1;
    ASSERT_TRUE(fr_player_drive_gpu_submit(drive,input,2,g_err,sizeof(g_err)));
    dc_gpu_world_body_t a,b;
    ASSERT_TRUE(dc_gpu_read_world_body(g_gpu,101,&a,g_err,sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g_gpu,102,&b,g_err,sizeof(g_err)));
    ASSERT_EQ(a.body.vx_fp,65536);ASSERT_EQ(b.body.vx_fp,-65536);
    input[0].sequence=UINT64_MAX;input[1].sequence=2;input[1].move_x=NAN;
    ASSERT_TRUE(!fr_player_drive_gpu_submit(drive,input,2,g_err,sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g_gpu,101,&a,g_err,sizeof(g_err)));
    ASSERT_EQ(a.body.vx_fp,65536);
    input[1].move_x=1;
    ASSERT_TRUE(fr_player_drive_gpu_submit(drive,input,2,g_err,sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g_gpu,101,&a,g_err,sizeof(g_err)));
    ASSERT_EQ(a.body.vx_fp,-65536);
    fr_player_drive_gpu_destroy(drive);PASS();
}

static void test_stale_generation_and_missing_body_do_not_control_replacements(void) {
    fr_player_drive_gpu_t *drive=NULL;
    dc_gpu_world_body_t before;
    ASSERT_TRUE(dc_gpu_read_world_body(g_gpu,101,&before,g_err,sizeof(g_err)));
    g_bindings[0].generation++;g_bindings[1].body_id=999;
    ASSERT_TRUE(fr_player_drive_gpu_create(g_gpu,g_bindings,2,1,2,&drive,g_err,sizeof(g_err)));
    fr_player_drive_input_t input[2]={{41,1,1,true},{42,1,1,true}};
    ASSERT_TRUE(fr_player_drive_gpu_submit(drive,input,2,g_err,sizeof(g_err)));
    dc_gpu_world_body_t b;
    ASSERT_TRUE(dc_gpu_read_world_body(g_gpu,101,&b,g_err,sizeof(g_err)));
    ASSERT_EQ(b.body.vx_fp,before.body.vx_fp); ASSERT_EQ(b.body.vy_fp,before.body.vy_fp);
    fr_player_drive_gpu_destroy(drive);PASS();
}

static void test_binding_validation_and_owner_input_routing(void) {
    fr_player_drive_gpu_t *drive=NULL;
    fr_player_body_binding_t same[2]={g_bindings[0],g_bindings[0]};
    ASSERT_TRUE(!fr_player_drive_gpu_create(g_gpu,same,2,1,2,&drive,g_err,sizeof(g_err)));
    ASSERT_TRUE(!drive);
    ASSERT_TRUE(!fr_player_drive_gpu_create(g_gpu,g_bindings,2,INFINITY,2,&drive,g_err,sizeof(g_err)));
    ASSERT_TRUE(fr_player_drive_gpu_create(g_gpu,g_bindings,2,1,2,&drive,g_err,sizeof(g_err)));
    fr_player_drive_input_t input[2]={{42,1,1,false},{41,1,1,false}};
    ASSERT_TRUE(!fr_player_drive_gpu_submit(drive,input,2,g_err,sizeof(g_err)));
    input[0].identity=41;input[1].identity=42;input[1].sequence=0;
    ASSERT_TRUE(!fr_player_drive_gpu_submit(drive,input,2,g_err,sizeof(g_err)));
    fr_player_drive_gpu_destroy(drive);PASS();
}

static void test_trimmed_canonical_binding_tracks_real_wet_hot_parcel_during_motion(void) {
    uint32_t ox=0,oy=0;bool found_region=false;
    for (uint32_t y=1;y<37 && !found_region;y++) for (uint32_t x=1;x<37 && !found_region;x++) {
        bool solid=true;
        for (uint32_t dy=0;dy<3;dy++) for (uint32_t dx=0;dx<3;dx++) {
            fr_beaver_source_t source;
            solid=solid && fr_beaver_source((y+dy)*40+x+dx+1,&source) && source.prop_material==2;
        }
        if (solid) {ox=x;oy=y;found_region=true;}
    }
    ASSERT_TRUE(found_region);
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for (uint32_t y=4;y<7;y++) for (uint32_t x=4;x<7;x++)
        chunk->cells[y*64+x].material=DC_MATERIAL_WOOD;
    ASSERT_TRUE(dc_gpu_upload_chunk(g_gpu,0,chunk,g_err,sizeof(g_err)));free(chunk);
    fr_reactive_state_t state;
    uint32_t mass=(uint32_t)lroundf(fr_reactive_profile(DC_MATERIAL_WOOD)->density*DC_FLUID_FULL);
    ASSERT_TRUE(fr_reactive_state_default(DC_MATERIAL_WOOD,mass,900,700,91,&state,g_err,sizeof(g_err)));
    state.ignition_threshold=.8f;state.ignition_integral=.4f;
    ASSERT_TRUE(fr_gpu_reactive_seed_cell(g_gpu,0,5*64+5,&state,g_err,sizeof(g_err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g_gpu) && dc_gpu_rigid_step(g_gpu,g_err,sizeof(g_err)));
    fr_reactive_record_t before,after;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g_gpu,0x80000001u,1,1,&before,&found,g_err,sizeof(g_err)) && found);
    uint32_t source=(oy+1)*40+ox+2;
    fr_appearance_owner_t appearance;fr_reactive_owner_t reactive;
    ASSERT_TRUE(fr_beaver_pixel_owners_trimmed(source,99,before.owner.key[0],before.owner.key[1],
        ox,oy,3,3,&appearance,&reactive));
    ASSERT_TRUE(memcmp(&reactive,&before.owner,sizeof(reactive))==0);
    ASSERT_EQ(appearance.x,ox+1);ASSERT_EQ(appearance.y,oy+1);
    fr_player_body_binding_t binding={99,before.owner.key[0],before.owner.key[1]};
    fr_player_drive_gpu_t *drive=NULL;
    ASSERT_TRUE(fr_player_drive_gpu_create(g_gpu,&binding,1,1,2,&drive,g_err,sizeof(g_err)));
    fr_player_drive_input_t input={99,1,1,false};
    ASSERT_TRUE(fr_player_drive_gpu_submit(drive,&input,1,g_err,sizeof(g_err)) &&
        dc_gpu_rigid_step(g_gpu,g_err,sizeof(g_err)));
    ASSERT_TRUE(fr_gpu_reactive_read_body(g_gpu,reactive.key[0],reactive.key[2],reactive.key[3],
        &after,&found,g_err,sizeof(g_err)) && found);
    ASSERT_TRUE(memcmp(&before,&after,sizeof(before))==0);
    fr_appearance_owner_t unchanged=appearance;fr_reactive_owner_t old=reactive;
    ASSERT_TRUE(!fr_beaver_pixel_owners_trimmed(source,99,binding.body_id,binding.generation,
        ox+2,oy,3,3,&appearance,&reactive));
    ASSERT_TRUE(memcmp(&appearance,&unchanged,sizeof(appearance))==0);
    ASSERT_TRUE(memcmp(&reactive,&old,sizeof(reactive))==0);
    fr_player_drive_gpu_destroy(drive);PASS();
}

int main(void) {
    struct {const char *name;void (*run)(void);} tests[]={
        {"real body movement and independent player",test_real_body_drive_preserves_position_and_other_player},
        {"jump from actual solver support",test_jump_uses_actual_solver_support_and_consumes_support},
        {"retry and batch validation",test_sequence_retries_and_invalid_batches_are_transactional},
        {"stale generation and missing body",test_stale_generation_and_missing_body_do_not_control_replacements},
        {"binding ownership validation",test_binding_validation_and_owner_input_routing},
        {"trimmed live reactive parcel",test_trimmed_canonical_binding_tracks_real_wet_hot_parcel_during_motion}};
    for (uint32_t i=0;i<sizeof(tests)/sizeof(*tests);i++) {
        if (!fixture()) {printf("Fixture FAIL: %s\n",g_err);g_fail++;}
        else {printf("RUN %s\n",tests[i].name);tests[i].run();}
        dc_gpu_destroy(g_gpu);g_gpu=NULL;
    }
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail?1:0;
}
