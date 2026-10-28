#define main rigid_solver_fixture_main
#include "../vulkan/rigid_solver_tests.c"
#undef main

#include "../../src/app/catapult_scene.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/thermal_gpu.h"
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
    for(uint32_t tick=0;tick<15;tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_world_body_t before,after;
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&before,err,sizeof(err)));
    float launch_speed=hypotf(before.body.vx_fp/65536.f,
                              before.body.vy_fp/65536.f);
    ASSERT_TRUE(dc_catapult_scene_release(g,&scene,err,sizeof(err)));
    ASSERT_TRUE(scene.released);
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&after,err,sizeof(err)));
    ASSERT_EQ(memcmp(&before,&after,sizeof(before)),0);
    for(uint32_t tick=0;tick<8;tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_world_body_t flight;
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&flight,err,sizeof(err)));
    float travel=hypotf(xof(flight)-xof(before),yof(flight)-yof(before));
    printf("coal release speed %.3f travel %.3f from %.2f %.2f to %.2f %.2f\n",
           launch_speed,travel,xof(before),yof(before),xof(flight),yof(flight));
    ASSERT_TRUE(launch_speed>.5f&&travel>2.f);
    ASSERT_TRUE(fr_gpu_read_structure_component(g,1,&support));
    ASSERT_EQ(support.state,FR_STRUCTURAL_SUPPORTED);
    for(uint32_t tick=0;tick<10;tick++) {
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    }
    dc_gpu_world_body_t impact;
    dc_gpu_body_motion_t arm_motion;
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&impact,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g,scene.arm_body,&arm_motion,err,sizeof(err)));
    ASSERT_TRUE(xof(impact)<xof(flight)-3.f&&yof(impact)>90.f);
    ASSERT_TRUE(fabsf(impact.body.vx_fp/65536.f)<launch_speed);
    ASSERT_TRUE(isfinite(arm_motion.angular_velocity));
    ASSERT_TRUE(fr_gpu_read_structure_component(g,1,&support));
    ASSERT_EQ(support.state,FR_STRUCTURAL_SUPPORTED);
    ASSERT_TRUE(isfinite(support.external[0])&&isfinite(support.external[1]));
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

static void test_hot_coal_emits_finite_reaction_without_igniting_cold_control(void) {
    char err[256]={0};
    for(uint32_t hot=0;hot<2;hot++) {
        dc_gpu_t *g=grid();ASSERT_TRUE(g);
        dc_catapult_scene_t scene={0};
        ASSERT_TRUE(dc_catapult_scene_build(g,hot!=0,&scene,err,sizeof(err)));
        fr_reactive_record_t before,after;bool found=false;
        ASSERT_TRUE(fr_gpu_reactive_read_body(g,scene.coal_body,0,1,&before,&found,
                                               err,sizeof(err))&&found);
        dc_gpu_world_body_t pose;
        fr_thermal_inspection_t probe;
        ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&pose,err,sizeof(err)));
        ASSERT_TRUE(fr_thermal_gpu_inspect(g,70,77,&probe,err,sizeof(err)));
        printf("coal pose %.2f %.2f thermal occupied %u material %u temp %.2f\n",
               xof(pose),yof(pose),probe.occupied,probe.material,probe.temperature_K);
        ASSERT_TRUE(fr_combustion_gpu_step(g,1.f/60.f,err,sizeof(err)));
        fr_combustion_emission_t *emission=calloc(DC_CHUNK_CELLS,sizeof(*emission));
        ASSERT_TRUE(emission);
        ASSERT_TRUE(fr_combustion_gpu_read_emission_chunk(g,3,emission,
            DC_CHUNK_CELLS,err,sizeof(err)));
        float peak=0.f;uint32_t peak_index=0;
        for(uint32_t i=0;i<DC_CHUNK_CELLS;i++) if(emission[i].power>peak) {
            peak=emission[i].power;peak_index=i;
        }
        printf("coal hot%u emission peak %.3f at %u\n",hot,peak,peak_index);
        for(uint32_t step=0;step<3;step++)
            ASSERT_TRUE(fr_combustion_gpu_step(g,1.f/60.f,err,sizeof(err)));
        ASSERT_TRUE(fr_gpu_reactive_read_body(g,scene.coal_body,0,1,&after,&found,
                                               err,sizeof(err))&&found);
        printf("coal hot%u fuel %u->%u char %u->%u flags %u->%u hazard %.3f->%.3f\n",
               hot,before.state.fuel_mass_fp,after.state.fuel_mass_fp,
               before.state.char_mass_fp,after.state.char_mass_fp,
               before.state.flags,after.state.flags,
               before.state.ignition_integral,after.state.ignition_integral);
        if(hot) {
            ASSERT_TRUE(after.state.fuel_mass_fp<before.state.fuel_mass_fp);
            ASSERT_TRUE(after.state.char_mass_fp>before.state.char_mass_fp);
            ASSERT_TRUE(after.state.inert_mass_fp>before.state.inert_mass_fp);
            ASSERT_TRUE(peak>0.f);
        } else {
            ASSERT_EQ(after.state.fuel_mass_fp,before.state.fuel_mass_fp);
            ASSERT_EQ(after.state.char_mass_fp,before.state.char_mass_fp);
            ASSERT_EQ(peak,0.f);
        }
        free(emission);
        dc_gpu_destroy(g);
    }
    PASS();
}

#ifndef DC_CATAPULT_DISPLAY_INCLUDE
int main(void) {
    RUN(test_cold_launcher_has_supported_frame_and_real_latch);
    RUN(test_hot_coal_load_keeps_finite_fuel_and_checkpointed_joint);
    RUN(test_hot_coal_emits_finite_reaction_without_igniting_cold_control);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
#endif
