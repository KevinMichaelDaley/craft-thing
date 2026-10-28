#define main rigid_solver_fixture_main
#include "../vulkan/rigid_solver_tests.c"
#undef main

#include "../../src/app/catapult_scene.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/physics/revolute_gpu.h"
#include "ferrum/structural/gpu.h"

static void test_cold_launcher_has_supported_frame_and_real_latch(void) {
    char err[256]={0};dc_gpu_t *g=grid();ASSERT_TRUE(g);
    dc_catapult_scene_t scene={0};
    bool built=dc_catapult_scene_build(g,false,&scene,err,sizeof(err));
    if(!built) printf("cold scene: %s\n",err);
    ASSERT_TRUE(built);
    ASSERT_TRUE(scene.frame_body&&scene.arm_body&&scene.coal_body&&
                scene.pivot_joint&&scene.latch_joint&&!scene.released);
    for(uint32_t tick=0;tick<5;tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    fr_structural_gpu_component_state_t support;
    ASSERT_TRUE(fr_gpu_read_structure_component(g,1,&support));
    printf("catapult support state %u flags %u residual %.3f, external %.3f %.3f\n",
           support.state,support.flags,support.relative_residual,
           support.external[0],support.external[1]);
    ASSERT_EQ(support.state,FR_STRUCTURAL_SUPPORTED);
    dc_gpu_body_pixel_t coal[8];uint32_t count=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,scene.coal_body,coal,8,&count,
                                        err,sizeof(err)));
    ASSERT_EQ(count,5u);
    for(uint32_t i=0;i<count;i++) ASSERT_EQ(coal[i].material,DC_GPU_BODY_COAL);
    dc_gpu_world_body_t before,after;
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&before,err,sizeof(err)));
    ASSERT_TRUE(dc_catapult_scene_release(g,&scene,err,sizeof(err)));
    ASSERT_TRUE(scene.released);
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&after,err,sizeof(err)));
    ASSERT_EQ(memcmp(&before,&after,sizeof(before)),0);
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_destroy(g);PASS();
}

static void test_hot_coal_load_keeps_finite_fuel_and_checkpointed_joint(void) {
    char err[256]={0};dc_gpu_t *g=grid();ASSERT_TRUE(g);
    dc_catapult_scene_t scene={0};
    bool built=dc_catapult_scene_build(g,true,&scene,err,sizeof(err));
    if(!built) printf("hot scene: %s\n",err);
    ASSERT_TRUE(built);
    fr_reactive_record_t coal;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,scene.coal_body,1,1,&coal,&found,
                                           err,sizeof(err))&&found);
    ASSERT_EQ(coal.state.material,DC_MATERIAL_COAL);
    ASSERT_TRUE(coal.state.fuel_mass_fp>0u);
    ASSERT_TRUE(coal.state.thermal_energy>0.f);
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/catapult_scene.dcs",err,sizeof(err)));
    dc_gpu_t *copy=grid();ASSERT_TRUE(copy);
    ASSERT_TRUE(fr_gpu_load_structure(copy,"build/catapult_scene.dcs",err,
                                      sizeof(err)));
    fr_reactive_record_t restored;
    ASSERT_TRUE(fr_gpu_reactive_read_body(copy,scene.coal_body,1,1,&restored,
                                           &found,err,sizeof(err))&&found);
    ASSERT_EQ(memcmp(&coal.state,&restored.state,sizeof(coal.state)),0);
    ASSERT_TRUE(dc_catapult_scene_release(copy,&scene,err,sizeof(err)));
    dc_gpu_destroy(copy);dc_gpu_destroy(g);PASS();
}

int main(void) {
    RUN(test_cold_launcher_has_supported_frame_and_real_latch);
    RUN(test_hot_coal_load_keeps_finite_fuel_and_checkpointed_joint);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
