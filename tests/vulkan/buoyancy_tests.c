#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dungeoncraft/gpu.h"

static int g_pass, g_fail;
#define RUN(fn) do { printf("RUN %s\n", #fn); fn(); } while (0)
#define ASSERT_TRUE(e) do { if (!(e)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #e); ++g_fail; return; } } while (0)
#define ASSERT_EQ(a,b) ASSERT_TRUE((a)==(b))
#define ASSERT_INT_EQ(a,b) ASSERT_TRUE((int)(a)==(int)(b))
#define PASS() ++g_pass

static dc_gpu_t *make_water_body_flow(uint32_t material, uint32_t top, bool wet, float flow) {
    char err[256] = {0};
    dc_chunk_t *chunk = calloc(1, sizeof(*chunk));
    dc_gpu_t *gpu = NULL;
    if (!chunk) return NULL;
    for (uint32_t y=0; y<64; ++y)
        for (uint32_t x=0; x<64; ++x) {
            dc_cell_t *cell=&chunk->cells[y*64+x];
            if (x==0 || x==63 || y==63) cell->material=DC_MATERIAL_STONE;
            else if (wet && y>=20) {
                cell->fluid_mass=DC_FLUID_FULL;
                chunk->face_velocity[y*64+x].x=flow;
            }
        }
    dc_gpu_body_t body={.x_fp=20<<16,.y_fp=(int32_t)(top<<16),
        .width=4,.height=4,.id=100,.active=1};
    dc_gpu_body_shape_t shape={.count=4,.material=material,
        .vertices={{0,0},{4<<16,0},{4<<16,4<<16},{0,4<<16}}};
    bool okay=dc_gpu_create(&gpu,64,64,"build/shaders/pattern.comp.spv",err,sizeof(err)) &&
        dc_gpu_upload_chunk(gpu,0,chunk,err,sizeof(err)) &&
        dc_gpu_set_page(gpu,0,0,0,err,sizeof(err)) &&
        dc_gpu_set_rigid_solver(gpu,true) &&
        dc_gpu_spawn_convex_body(gpu,(dc_gpu_world_body_t){.body=body},&shape,err,sizeof(err)) &&
        dc_gpu_set_body_origin(gpu,(dc_chunk_coord_t){0,0},err,sizeof(err));
    free(chunk);
    if (!okay) { printf("setup: %s\n",err); dc_gpu_destroy(gpu); return NULL; }
    return gpu;
}

static dc_gpu_t *make_water_body(uint32_t material, uint32_t top, bool wet) {
    return make_water_body_flow(material, top, wet, 0);
}

static void test_dry_wood_falls(void) {
    char err[256]; dc_gpu_t *gpu=make_water_body(DC_GPU_BODY_WOOD,18,false);
    ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
    dc_gpu_body_t body;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu,100,&body,err,sizeof(err)));
    ASSERT_EQ(body.vy_fp,16384);
    dc_gpu_destroy(gpu); PASS();
}

static void test_submerged_wood_accelerates_from_material_density(void) {
    char err[256]; dc_gpu_t *gpu=make_water_body(DC_GPU_BODY_WOOD,30,true);
    ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
    dc_gpu_body_t body;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu,100,&body,err,sizeof(err)));
    ASSERT_TRUE(fabsf((float)body.vy_fp/65536.f-(.25f-.25f/.6f))<.002f);
    dc_gpu_destroy(gpu); PASS();
}

static void test_partial_wood_uses_submerged_shape_volume(void) {
    char err[256]; dc_gpu_t *gpu=make_water_body(DC_GPU_BODY_WOOD,18,true);
    ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
    dc_gpu_body_t body;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu,100,&body,err,sizeof(err)));
    ASSERT_TRUE(fabsf((float)body.vy_fp/65536.f-(.25f-.125f/.6f))<.002f);
    dc_gpu_destroy(gpu); PASS();
}

static void test_submerged_stone_sinks_with_reduced_acceleration(void) {
    char err[256]; dc_gpu_t *gpu=make_water_body(DC_GPU_BODY_STONE,30,true);
    ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
    dc_gpu_body_t body;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu,100,&body,err,sizeof(err)));
    ASSERT_TRUE(fabsf((float)body.vy_fp/65536.f-(.25f-.25f/2.7f))<.002f);
    dc_gpu_destroy(gpu); PASS();
}

static void test_dense_wood_sinks_from_its_material_properties(void) {
    char err[256]; dc_gpu_t *gpu=make_water_body(4u,30,true);
    ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
    dc_gpu_body_t body;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu,100,&body,err,sizeof(err)));
    ASSERT_TRUE(fabsf((float)body.vy_fp/65536.f-(.25f-.25f/1.2f))<.002f);
    dc_gpu_destroy(gpu); PASS();
}

static void test_asymmetric_immersion_applies_buoyancy_torque(void) {
    char err[256]; dc_gpu_t *gpu=make_water_body(DC_GPU_BODY_WOOD,18,true);
    ASSERT_TRUE(gpu);
    dc_gpu_world_body_t wide={.body={.x_fp=20<<16,.y_fp=18<<16,
        .width=8,.height=4,.id=100,.active=1}};
    dc_gpu_body_shape_t shape={.count=4,.material=DC_GPU_BODY_WOOD,
        .vertices={{0,0},{8<<16,0},{8<<16,4<<16},{0,4<<16}}};
    ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu,wide,&shape,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(gpu,100,.3f,0,0,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
    dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu,100,&motion,err,sizeof(err)));
    printf("wide waterline restoring omega %f\n",motion.angular_velocity);
    ASSERT_TRUE(motion.angular_velocity < -.001f);
    dc_gpu_destroy(gpu); PASS();
}

static void test_submerged_body_follows_fluid_velocity(void) {
    char err[256]; dc_gpu_t *gpu=make_water_body_flow(DC_GPU_BODY_WOOD,30,true,.5f);
    ASSERT_TRUE(gpu);
    ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
    dc_gpu_body_t body;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu,100,&body,err,sizeof(err)));
    ASSERT_TRUE(body.vx_fp > 0 && body.vx_fp < 32768);
    dc_gpu_destroy(gpu); PASS();
}

static void test_wood_converges_to_repeatable_density_derived_draft(void) {
    char err[256]; dc_gpu_t *gpu=make_water_body(DC_GPU_BODY_WOOD,18,true);
    ASSERT_TRUE(gpu);
    dc_gpu_world_body_t wide={.body={.x_fp=20<<16,.y_fp=18<<16,
        .width=8,.height=4,.id=100,.active=1}};
    dc_gpu_body_shape_t shape={.count=4,.material=DC_GPU_BODY_WOOD,
        .vertices={{0,0},{8<<16,0},{8<<16,4<<16},{0,4<<16}}};
    ASSERT_TRUE(dc_gpu_spawn_convex_body(gpu,wide,&shape,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_origin(gpu,(dc_chunk_coord_t){0,0},err,sizeof(err)));
    for (uint32_t tick=0; tick<240; ++tick) {
        ASSERT_TRUE(dc_gpu_rigid_step(gpu,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_fluid_step(gpu,err,sizeof(err)));
    }
    dc_gpu_body_t body; dc_gpu_body_motion_t motion;
    ASSERT_TRUE(dc_gpu_read_body_id(gpu,100,&body,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(gpu,100,&motion,err,sizeof(err)));
    dc_chunk_t *saved=calloc(1,sizeof(*saved)); ASSERT_TRUE(saved);
    ASSERT_TRUE(dc_gpu_download_chunk(gpu,0,saved,err,sizeof(err)));
    uint64_t water=0, column=0; float surface=0;
    for (uint32_t i=0; i<DC_CHUNK_CELLS; ++i) water+=saved->cells[i].fluid_mass;
    for (uint32_t y=0; y<63; ++y) column+=saved->cells[y*64+19].fluid_mass;
    surface=63.f-(float)column/65536.f;
    float draft=(float)body.y_fp/65536.f+4.f-surface;
    printf("wood draft %.5f surface %.5f y %.5f angle %.5f vy %d water %llu\n",draft,surface,(float)body.y_fp/65536.f,motion.angle,body.vy_fp,(unsigned long long)water);
    ASSERT_EQ(water,43u*62u*(uint64_t)DC_FLUID_FULL);
    ASSERT_TRUE(fabsf(draft-2.4f)<.2f);
    ASSERT_TRUE(abs(body.vy_fp)<1024);
    free(saved); dc_gpu_destroy(gpu); PASS();
}

int main(void) {
    RUN(test_dry_wood_falls);
    RUN(test_submerged_wood_accelerates_from_material_density);
    RUN(test_partial_wood_uses_submerged_shape_volume);
    RUN(test_submerged_stone_sinks_with_reduced_acceleration);
    RUN(test_dense_wood_sinks_from_its_material_properties);
    RUN(test_asymmetric_immersion_applies_buoyancy_torque);
    RUN(test_submerged_body_follows_fluid_velocity);
    RUN(test_wood_converges_to_repeatable_density_derived_draft);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail ? 1 : 0;
}
