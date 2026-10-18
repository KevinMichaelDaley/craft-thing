#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../src/vulkan/gpu_internal.h"

static int g_pass,g_fail;
#define RUN(fn) do {printf("RUN %s\n",#fn);fn();} while(0)
#define ASSERT_TRUE(e) do {if(!(e)){printf("FAIL %d: %s\n",__LINE__,#e);g_fail++;return;}} while(0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() g_pass++

static void test_supported_stump_stays_terrain_after_trunk_cut(void) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned y=20;y<=30;y++)
        chunk->cells[y*64+30]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    chunk->cells[31*64+30].material=DC_MATERIAL_DIRT;
    chunk->cells[32*64+30]=(dc_cell_t){.material=DC_MATERIAL_STONE,.flags=DC_CELL_LOADED_TERRAIN};
    chunk->cells[22*64+29].material=DC_MATERIAL_DIRT;
    chunk->cells[23*64+29]=chunk->cells[23*64+30];
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,30,25,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    for(unsigned y=26;y<=30;y++) ASSERT_EQ(chunk->cells[y*64+30].material,DC_MATERIAL_WOOD);
    for(unsigned y=20;y<25;y++) ASSERT_EQ(chunk->cells[y*64+30].material,DC_MATERIAL_AIR);
    dc_gpu_destroy(g);free(chunk);PASS();
}

static void test_partial_granular_support_motion_does_not_release_tree_but_cut_does(void) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    chunk->cells[29*64+30]=(dc_cell_t){.material=DC_MATERIAL_WOOD,.flags=DC_CELL_LOADED_TERRAIN};
    chunk->cells[30*64+30]=chunk->cells[29*64+30];
    chunk->cells[29*64+29].material=DC_MATERIAL_DIRT;
    chunk->cells[30*64+29].material=DC_MATERIAL_DIRT;
    chunk->cells[31*64+30].material=DC_MATERIAL_DIRT;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_enable_automatic_components(g));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    unsigned removed=30*64+29;
    chunk->cells[removed]=(dc_cell_t){0};chunk->particles[removed]=(dc_mpm_particle_t){0};chunk->particle_count--;
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_t body;
    ASSERT_TRUE(!dc_gpu_read_body_id(g,0x80000001u,&body,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,29,29,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(!dc_gpu_read_body_id(g,0x80000001u,&body,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_paint_material(g,30,30,0,DC_MATERIAL_AIR,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_pixel_t pixel;unsigned count=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,0x80000001u,&pixel,1,&count,err,sizeof(err)));
    ASSERT_EQ(count,1u);dc_gpu_destroy(g);free(chunk);PASS();
}

static void test_full_soil_and_gravel_remain_packed_after_settling(void) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    for(unsigned material=DC_MATERIAL_DIRT;material<=DC_MATERIAL_GRAVEL;material++) {
        *chunk=(dc_chunk_t){0};
        for(unsigned y=0;y<64;y++) for(unsigned x=0;x<64;x++) {
            if(y>=48||x==23||x==40) chunk->cells[y*64+x].material=DC_MATERIAL_STONE;
            else if(y>=24&&x>=24&&x<40) chunk->cells[y*64+x].material=material;
        }
        ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
        for(unsigned tick=0;tick<64;tick++) ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
        unsigned filled=0,doubled=0;uint64_t mass=0;
        for(unsigned i=0;i<DC_CHUNK_CELLS;i++) {
            filled+=chunk->cells[i].material==material;
            doubled+=chunk->particles[i+4096].mass_fp!=0;
            mass+=chunk->particles[i].mass_fp+chunk->particles[i+4096].mass_fp;
        }
        printf("material%u packed cells%u secondary%u mass%llu\n",material,filled,doubled,(unsigned long long)mass);
        ASSERT_EQ(mass,(uint64_t)384*(material==DC_MATERIAL_DIRT?78643u:176947u));
        ASSERT_EQ(filled,384u);ASSERT_EQ(doubled,0u);
    }
    dc_gpu_destroy(g);free(chunk);PASS();
}

static void test_dense_leaf_contacts_cannot_weaken_stone_floor(void) {
    dc_gpu_t *g=NULL;char err[256];dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned y=48;y<64;y++) for(unsigned x=0;x<64;x++) chunk->cells[y*64+x].material=DC_MATERIAL_STONE;
    for(unsigned y=6;y<48;y++) for(unsigned x=28;x<36;x++) {
        unsigned cell=y*64+x;chunk->cells[cell].material=DC_MATERIAL_LEAF_DEBRIS;
        dc_chunk_particle_init(chunk->particles+cell,chunk->coord,cell,DC_MATERIAL_LEAF_DEBRIS);
        chunk->particles[cell+4096]=chunk->particles[cell];
        chunk->particles[cell+4096].id_lo+=4096;chunk->particle_count+=2;
    }
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_gpu_body_shape_t shape={.count=4,.material=DC_GPU_BODY_WOOD,
        .vertices={{0,0},{6*65536,0},{6*65536,16*65536},{0,16*65536}}};
    dc_gpu_body_t body={.id=77,.active=1,.x_fp=29*65536,.y_fp=6*65536,.width=6,.height=16};
    dc_gpu_world_body_t world_body={.body=body};
    ASSERT_TRUE(dc_gpu_spawn_convex_body(g,world_body,&shape,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,77,0,0,2,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    float deepest=0;
    for(unsigned tick=0;tick<80;tick++) {
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
        dc_gpu_body_record_t *b=g->body_mapped;
        float bottom=b->body.y_fp/65536.f+b->center[1]+fabsf(sinf(b->angle))*3+fabsf(cosf(b->angle))*8;
        if(bottom>deepest) deepest=bottom;
    }
    printf("dense-leaf floor deepest body point %.4f (floor48)\n",deepest);
    ASSERT_TRUE(deepest>46.f);ASSERT_TRUE(deepest<48.2f);
    dc_gpu_destroy(g);free(chunk);PASS();
}

static void test_grounded_powders_bear_rigid_loads(void) {
    const unsigned materials[]={DC_MATERIAL_DIRT,DC_MATERIAL_GRAVEL,DC_MATERIAL_SAND};
    bool held=true;
    char err[256];dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    for(unsigned trial=0;trial<8;trial++) {
        unsigned material=materials[trial==7?0:trial%3];
        *chunk=(dc_chunk_t){0};
        for(unsigned y=24;y<64;y++) for(unsigned x=0;x<64;x++)
            chunk->cells[y*64+x].material=y>=56?DC_MATERIAL_STONE:material;
        ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
        for(unsigned tick=0;tick<90;tick++) ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
        unsigned surface=56;
        for(unsigned y=0;y<56;y++) {
            unsigned occupied=0;
            for(unsigned x=28;x<36;x++) occupied+=chunk->cells[y*64+x].material==material;
            if(occupied==8&&y<surface) surface=y;
        }
        ASSERT_TRUE(surface<56);
        if(material!=DC_MATERIAL_SAND) surface=24;
        float grain_surface=64;
        for(unsigned i=0;i<DC_MPM_PARTICLES_PER_CHUNK;i++) {
            dc_mpm_particle_t p=chunk->particles[i];
            if(p.mass_fp&&p.x_fp>=28*65536&&p.x_fp<36*65536)
                grain_surface=fminf(grain_surface,(p.y_fp-(int)p.grain_fp)/65536.f);
        }
        dc_gpu_body_shape_t shape={.count=4,.material=trial<3?DC_GPU_BODY_WOOD:DC_GPU_BODY_STONE,
            .vertices={{0,0},{8*65536,0},{8*65536,8*65536},{0,8*65536}}};
        dc_gpu_world_body_t body={.body={.id=77,.active=1,.x_fp=28*65536,.y_fp=(surface-12)*65536,.width=8,.height=8}};
        if(trial==7) {
            dc_gpu_compound_shape_t compound={.count=2,.pieces={
                {.count=4,.material=DC_GPU_BODY_DENSE_WOOD,.vertices={{0,0},{4*65536,0},{4*65536,8*65536},{0,8*65536}}},
                {.count=4,.material=DC_GPU_BODY_DENSE_WOOD,.vertices={{4*65536,0},{8*65536,0},{8*65536,8*65536},{4*65536,8*65536}}}}};
            ASSERT_TRUE(dc_gpu_spawn_compound_body(g,body,&compound,err,sizeof(err)));
        } else ASSERT_TRUE(dc_gpu_spawn_convex_body(g,body,&shape,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
        ASSERT_TRUE(dc_gpu_set_body_motion(g,77,0,0,2,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
        if(trial==6) {
            dc_gpu_body_record_t *record=g->body_mapped;
            record->mass*=1000;record->density*=1000;record->inertia*=1000;
            record->inverse_mass/=1000;record->inverse_inertia/=1000;
            record->body.y_fp=record->local_y_fp=(int)((grain_surface-8)*65536);
        }
        float fastest=0;int previous=trial==6?(int)((grain_surface-8)*65536):(int)(surface-12)*65536;
        for(unsigned tick=0;tick<120;tick++) {
            ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
            dc_gpu_body_t current;ASSERT_TRUE(dc_gpu_read_body_id(g,77,&current,err,sizeof(err)));
            if(trial==6&&current.y_fp>previous) fastest=fmaxf(fastest,(current.y_fp-previous)/65536.f);
            if(trial==6&&current.y_fp-previous>6553) printf("overload tick%u descent%.4f\n",tick,(current.y_fp-previous)/65536.f);
            previous=current.y_fp;
        }
        dc_gpu_body_t result;ASSERT_TRUE(dc_gpu_read_body_id(g,77,&result,err,sizeof(err)));
        printf("powder%u %s body bottom %.4f (settled surface%u), overload max step%.4f\n",material,trial<3?"wood":trial==6?"1000x stone":trial==7?"compound wood":"stone",result.y_fp/65536.+8,surface,fastest);
        ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
        uint64_t mass=0;for(unsigned i=0;i<DC_MPM_PARTICLES_PER_CHUNK;i++) mass+=chunk->particles[i].mass_fp;
        ASSERT_EQ(mass,(uint64_t)2048*(material==DC_MATERIAL_DIRT?78643u:material==DC_MATERIAL_GRAVEL?176947u:65536u));
        held=held&&fastest<.1f&&(trial==6?result.y_fp>(int)(surface-8)*65536:
            result.y_fp<(int)(surface-6)*65536);
        ASSERT_TRUE(dc_gpu_remove_body(g,77,err,sizeof(err)));
    }
    dc_gpu_destroy(g);free(chunk);
    ASSERT_TRUE(held);PASS();
}
static void test_embedded_tree_starts_in_equilibrium_without_soil_jacking(void) {
    char err[256];dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned y=24;y<64;y++) for(unsigned x=0;x<64;x++)
        if(!(y<26&&x>=31&&x<33)) chunk->cells[y*64+x].material=y>=56?DC_MATERIAL_STONE:DC_MATERIAL_DIRT;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_gpu_body_shape_t shape={.count=4,.material=DC_GPU_BODY_WOOD,
        .vertices={{0,0},{2*65536,0},{2*65536,16*65536},{0,16*65536}}};
    dc_gpu_world_body_t body={.body={.id=77,.active=1,.x_fp=31*65536,.y_fp=10*65536+32768,.width=2,.height=16}};
    ASSERT_TRUE(dc_gpu_spawn_convex_body(g,body,&shape,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,77,0,0,2,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    float highest=10.5f,lowest=10.5f,final_speed=0,settled_y=0,final_y=0;
    for(unsigned tick=0;tick<360;tick++) {
        ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
        dc_gpu_body_t result;ASSERT_TRUE(dc_gpu_read_body_id(g,77,&result,err,sizeof(err)));
        highest=fminf(highest,result.y_fp/65536.f);lowest=fmaxf(lowest,result.y_fp/65536.f);
        final_speed=fabsf(result.vy_fp/65536.f);
        final_y=result.y_fp/65536.f;
        if(tick==179) settled_y=final_y;
    }
    printf("embedded trunk equilibrium y10.5 range%.4f..%.4f\n",highest,lowest);
    printf("late equilibrium drift %.6f final speed%.6f\n",final_y-settled_y,final_speed);
    ASSERT_TRUE(dc_gpu_download_chunk(g,0,chunk,err,sizeof(err)));
    uint64_t mass=0;unsigned refilled=0;
    for(unsigned i=0;i<DC_MPM_PARTICLES_PER_CHUNK;i++) mass+=chunk->particles[i].mass_fp;
    for(unsigned y=24;y<26;y++) for(unsigned x=31;x<33;x++) refilled+=chunk->cells[y*64+x].material==DC_MATERIAL_DIRT;
    dc_gpu_destroy(g);free(chunk);
    ASSERT_TRUE(highest>10.25f);ASSERT_TRUE(lowest<11.25f);
    ASSERT_TRUE(fabsf(final_y-settled_y)<.05f);
    ASSERT_TRUE(final_speed<.01f);ASSERT_EQ(refilled,0u);ASSERT_EQ(mass,(uint64_t)2044*78643u);PASS();
}
static void test_trapped_grains_cannot_bridge_a_clear_air_row(void) {
    char err[256];dc_gpu_t *g=NULL;dc_chunk_t *chunk=calloc(1,sizeof(*chunk));ASSERT_TRUE(chunk);
    for(unsigned y=28;y<64;y++) for(unsigned x=0;x<64;x++)
        chunk->cells[y*64+x].material=y>=56?DC_MATERIAL_STONE:DC_MATERIAL_DIRT;
    chunk->cells[26*64+31].material=chunk->cells[26*64+32].material=DC_MATERIAL_DIRT;
    ASSERT_TRUE(dc_gpu_create(&g,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_upload_chunk(g,0,chunk,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_page(g,0,0,0,err,sizeof(err)));
    dc_gpu_body_shape_t shape={.count=4,.material=DC_GPU_BODY_WOOD,
        .vertices={{0,0},{2*65536,0},{2*65536,16*65536},{0,16*65536}}};
    dc_gpu_world_body_t body={.body={.id=77,.active=1,.x_fp=31*65536,.y_fp=10*65536+32768,.width=2,.height=16}};
    ASSERT_TRUE(dc_gpu_spawn_convex_body(g,body,&shape,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,77,0,0,2,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(g,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    for(unsigned tick=0;tick<24;tick++) ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
    dc_gpu_body_t result;ASSERT_TRUE(dc_gpu_read_body_id(g,77,&result,err,sizeof(err)));
    printf("air-gap trunk y%.4f (initial10.5)\n",result.y_fp/65536.f);
    dc_gpu_destroy(g);free(chunk);ASSERT_TRUE(result.y_fp>11*65536);PASS();
}
int main(void) {
    RUN(test_supported_stump_stays_terrain_after_trunk_cut);
    RUN(test_trapped_grains_cannot_bridge_a_clear_air_row);
    RUN(test_embedded_tree_starts_in_equilibrium_without_soil_jacking);
    if(getenv("DC_EQUILIBRIUM_ONLY")) {
        printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
    }
    RUN(test_grounded_powders_bear_rigid_loads);
    RUN(test_partial_granular_support_motion_does_not_release_tree_but_cut_does);
    RUN(test_full_soil_and_gravel_remain_packed_after_settling);
    RUN(test_dense_leaf_contacts_cannot_weaken_stone_floor);
    printf("%d passed, %d failed\n",g_pass,g_fail);return g_fail!=0;
}
