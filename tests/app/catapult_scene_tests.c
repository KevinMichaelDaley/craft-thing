#define main rigid_solver_fixture_main
#include "../vulkan/rigid_solver_tests.c"
#undef main

#include "../../src/app/catapult_scene.h"
#include "ferrum/fire/gpu.h"
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/gas_gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/physics/revolute_gpu.h"
#include "ferrum/structural/gpu.h"

static void test_grounded_triangular_frame_and_tension_release(void) {
    char err[256]={0};dc_gpu_t *g=grid();ASSERT_TRUE(g);
    dc_catapult_scene_t scene={0};
    ASSERT_TRUE(dc_catapult_scene_build(g,false,&scene,err,sizeof(err)));
    ASSERT_TRUE(scene.pouch_joint&&scene.latch_joint);
    dc_gpu_world_body_t base,top,wall,coal_before,coal_after;
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.base_body,&base,
        err,sizeof(err)));
    dc_gpu_body_shape_t shape;
    ASSERT_TRUE(dc_gpu_read_body_shape(g,scene.base_body,&shape,
        err,sizeof(err)));
    ASSERT_EQ(shape.count,3u);
    ASSERT_TRUE(shape.vertices[0].y_fp>shape.vertices[1].y_fp&&
                shape.vertices[2].y_fp>shape.vertices[1].y_fp);
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.frame_body,&top,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.wall_body,&wall,err,sizeof(err)));
    printf("base %.2f %.2f frame %.2f %.2f wall %.2f %.2f\n",
        xof(base),yof(base),xof(top),yof(top),xof(wall),yof(wall));
    ASSERT_TRUE(xof(base)<xof(wall)-10.f);
    ASSERT_TRUE(yof(base)<yof(wall));
    dc_gpu_body_motion_t arm_held,arm_launched;
    for(uint32_t tick=0;tick<20;tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g,scene.arm_body,&arm_held,
        err,sizeof(err)));
    printf("arm held angle %.3f omega %.3f\n",arm_held.angle,
        arm_held.angular_velocity);
    ASSERT_TRUE(fabsf(arm_held.angular_velocity)<.05f);
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&coal_before,
        err,sizeof(err)));
    dc_gpu_world_body_t held_arm;
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.arm_body,&held_arm,
        err,sizeof(err)));
    printf("held arm %.2f %.2f size %u %u coal %.2f %.2f\n",xof(held_arm),
        yof(held_arm),held_arm.body.width,held_arm.body.height,
        xof(coal_before),yof(coal_before));
    ASSERT_TRUE(dc_catapult_scene_release(g,&scene,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&coal_after,
        err,sizeof(err)));
    ASSERT_EQ(memcmp(&coal_before,&coal_after,sizeof(coal_before)),0);
    for(uint32_t tick=0;tick<1;tick++) {
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
        dc_gpu_body_motion_t motion;
        dc_gpu_world_body_t projectile;
        ASSERT_TRUE(dc_gpu_read_body_motion(g,scene.arm_body,&motion,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&projectile,err,sizeof(err)));
        printf("launch tick %u angle %.3f omega %.3f coal %.2f %.2f velocity %.2f %.2f\n",
            tick,motion.angle,motion.angular_velocity,xof(projectile),yof(projectile),
            projectile.body.vx_fp/65536.f,projectile.body.vy_fp/65536.f);
    }
    ASSERT_TRUE(dc_gpu_read_body_motion(g,scene.arm_body,&arm_launched,
        err,sizeof(err)));
    printf("arm launched angle %.3f omega %.3f\n",arm_launched.angle,
        arm_launched.angular_velocity);
    dc_gpu_world_body_t pouch_ready;
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&pouch_ready,
        err,sizeof(err)));
    printf("pouch ready coal %.2f %.2f vel %.2f %.2f\n",
        xof(pouch_ready),yof(pouch_ready),
        pouch_ready.body.vx_fp/65536.f,pouch_ready.body.vy_fp/65536.f);
    ASSERT_TRUE(fabsf(arm_launched.angular_velocity)>.1f);
    ASSERT_TRUE(dc_catapult_scene_release_projectile(g,&scene,err,sizeof(err)));
    ASSERT_TRUE(scene.projectile_released);
    dc_gpu_world_body_t projectile_flight;
    for(uint32_t tick=0;tick<8;tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&projectile_flight,
        err,sizeof(err)));
    printf("projectile after release %.2f %.2f speed %.2f %.2f\n",
        xof(projectile_flight),yof(projectile_flight),
        projectile_flight.body.vx_fp/65536.f,
        projectile_flight.body.vy_fp/65536.f);
    dc_gpu_destroy(g);PASS();
}

static void test_cold_launcher_has_supported_frame_and_real_latch(void) {
    char err[256]={0};dc_gpu_t *g=grid();ASSERT_TRUE(g);
    dc_catapult_scene_t scene={0};
    bool built=dc_catapult_scene_build(g,false,&scene,err,sizeof(err));
    if(!built) printf("cold scene: %s\n",err);
    ASSERT_TRUE(built);
    ASSERT_TRUE(scene.frame_body&&scene.wall_body&&scene.arm_body&&scene.coal_body&&
                scene.pivot_joint&&scene.latch_joint&&!scene.released);
    dc_gpu_world_body_t wall;
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.wall_body,&wall,err,sizeof(err)));
    ASSERT_TRUE(wall.body.active);
    dc_gpu_body_shape_t wall_shape;
    ASSERT_TRUE(dc_gpu_read_body_shape(g,scene.wall_body,&wall_shape,
        err,sizeof(err)));
    ASSERT_EQ(wall_shape.material,DC_GPU_BODY_STONE);
    for(uint32_t tick=0;tick<5;tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    fr_structural_gpu_component_state_t support;
    ASSERT_TRUE(fr_gpu_read_structure_component(g,1,&support));
    printf("catapult support state %u flags %u residual %.3f, external %.3f %.3f\n",
           support.state,support.flags,support.relative_residual,
           support.external[0],support.external[1]);
    ASSERT_EQ(support.state,FR_STRUCTURAL_SUPPORTED);
    ASSERT_TRUE(isfinite(support.external[1]));
    fr_structural_gpu_component_state_t wall_rest;
    ASSERT_TRUE(fr_gpu_read_structure_component(g,3,&wall_rest));
    ASSERT_EQ(wall_rest.state,FR_STRUCTURAL_SUPPORTED);
    dc_gpu_body_pixel_t coal[8];uint32_t count=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,scene.coal_body,coal,8,&count,
                                        err,sizeof(err)));
    ASSERT_EQ(count,5u);
    for(uint32_t i=0;i<count;i++) ASSERT_EQ(coal[i].material,DC_GPU_BODY_COAL);
    for(uint32_t tick=0;tick<15;tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_world_body_t before,after;
    dc_gpu_body_motion_t arm_before;
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&before,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g,scene.arm_body,&arm_before,err,sizeof(err)));
    ASSERT_TRUE(fabsf(arm_before.angular_velocity)<.05f);
    ASSERT_TRUE(dc_catapult_scene_release(g,&scene,err,sizeof(err)));
    ASSERT_TRUE(scene.released);
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&after,err,sizeof(err)));
    ASSERT_EQ(memcmp(&before,&after,sizeof(before)),0);
    ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_motion_t arm_flight;
    ASSERT_TRUE(dc_gpu_read_body_motion(g,scene.arm_body,&arm_flight,
        err,sizeof(err)));
    ASSERT_TRUE(arm_flight.angle>arm_before.angle+.1f);
    ASSERT_TRUE(arm_flight.angular_velocity>.1f);
    dc_gpu_world_body_t launch;
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&launch,
        err,sizeof(err)));
    ASSERT_TRUE(launch.body.vx_fp/65536.f>.5f);
    ASSERT_TRUE(dc_catapult_scene_release_projectile(g,&scene,err,sizeof(err)));
    ASSERT_TRUE(scene.projectile_released);
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&after,
        err,sizeof(err)));
    ASSERT_EQ(memcmp(&launch,&after,sizeof(launch)),0);
    for(uint32_t tick=0;tick<8;tick++)
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_world_body_t flight;
    ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&flight,err,sizeof(err)));
    float travel=hypotf(xof(flight)-xof(before),yof(flight)-yof(before));
    printf("coal travel %.3f from %.2f %.2f to %.2f %.2f\n",
           travel,xof(before),yof(before),xof(flight),yof(flight));
    ASSERT_TRUE(travel>5.f&&xof(flight)>84.f&&yof(flight)>78.f&&
                yof(flight)<82.f);
    ASSERT_TRUE(isfinite(arm_flight.angular_velocity));
    ASSERT_TRUE(fr_gpu_read_structure_component(g,1,&support));
    ASSERT_EQ(support.state,FR_STRUCTURAL_SUPPORTED);
    fr_structural_gpu_component_state_t wall_support;
    ASSERT_TRUE(fr_gpu_read_structure_component(g,3,&wall_support));
    printf("wall support state %u flags %u load %.2f %.2f\n",
        wall_support.state,wall_support.flags,wall_support.external[0],
        wall_support.external[1]);
    ASSERT_EQ(wall_support.state,FR_STRUCTURAL_SUPPORTED);
    ASSERT_TRUE(wall_support.external[1]>wall_rest.external[1]*1.05f);
    ASSERT_TRUE(arm_flight.mass>0.f&&arm_flight.inertia>0.f);
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
    fr_reactive_record_t arm,arm_restored;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,scene.arm_body,13,0,&arm,&found,
        err,sizeof(err))&&found);
    ASSERT_TRUE(fr_gpu_reactive_read_body(copy,scene.arm_body,13,0,
        &arm_restored,&found,err,sizeof(err))&&found);
    ASSERT_EQ(memcmp(&arm.state,&arm_restored.state,sizeof(arm.state)),0);
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
        ASSERT_TRUE(fr_thermal_gpu_inspect(g,84,76,&probe,err,sizeof(err)));
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

static void test_wooden_arm_has_finite_reactive_ownership(void) {
    char err[256]={0};dc_gpu_t *g=grid();ASSERT_TRUE(g);
    dc_catapult_scene_t scene={0};
    ASSERT_TRUE(dc_catapult_scene_build(g,false,&scene,err,sizeof(err)));
    dc_gpu_body_pixel_t wood[40];uint32_t count=0;
    ASSERT_TRUE(dc_gpu_read_pixel_shape(g,scene.arm_body,wood,40,&count,
        err,sizeof(err)));
    ASSERT_EQ(count,32u);
    uint32_t minx=999,maxx=0,miny=999,maxy=0;
    for(uint32_t i=0;i<count;i++) {
        if(wood[i].x<minx) minx=wood[i].x;
        if(wood[i].x>maxx) maxx=wood[i].x;
        if(wood[i].y<miny) miny=wood[i].y;
        if(wood[i].y>maxy) maxy=wood[i].y;
    }
    printf("arm shape local bounds %u..%u %u..%u\n",minx,maxx,miny,maxy);
    for(uint32_t i=0;i<count;i++) ASSERT_EQ(wood[i].material,DC_GPU_BODY_WOOD);
    fr_reactive_record_t arm;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,scene.arm_body,1,0,&arm,&found,
        err,sizeof(err))&&found);
    ASSERT_EQ(arm.state.material,DC_MATERIAL_WOOD);
    ASSERT_TRUE(arm.state.fuel_mass_fp>0u);
    dc_gpu_destroy(g);PASS();
}

static void test_prompt_hot_launch_keeps_sound_wood_arm_unignited(void) {
    char err[256]={0};dc_gpu_t *g=grid();ASSERT_TRUE(g);
    dc_catapult_scene_t scene={0};
    ASSERT_TRUE(dc_catapult_scene_build(g,true,&scene,err,sizeof(err)));
    for(uint32_t tick=0;tick<20;tick++)
        ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_catapult_scene_release(g,&scene,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
    ASSERT_TRUE(dc_catapult_scene_release_projectile(g,&scene,err,sizeof(err)));
    for(uint32_t tick=0;tick<7;tick++)
        ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
    fr_reactive_record_t arm;bool found=false;
    ASSERT_TRUE(fr_gpu_reactive_read_body(g,scene.arm_body,13,0,&arm,&found,
        err,sizeof(err))&&found);
    float temperature=0.f;
    ASSERT_TRUE(fr_reactive_state_temperature(&arm.state,&temperature,
        err,sizeof(err)));
    printf("hot launcher arm %.1fK flags %u fuel %u\n",temperature,
        arm.state.flags,arm.state.fuel_mass_fp);
    ASSERT_TRUE((arm.state.flags&FR_REACTIVE_FLAMING)==0u);
    fr_structural_gpu_component_state_t support;
    ASSERT_TRUE(fr_gpu_read_structure_component(g,1,&support));
    ASSERT_EQ(support.state,FR_STRUCTURAL_SUPPORTED);
    dc_gpu_destroy(g);PASS();
}

static void test_equal_elapsed_cadences_keep_launcher_flight_and_gas_clock(void) {
    char err[256]={0};
    float position[3][2],angle[3];
    for(uint32_t cadence=0;cadence<3;cadence++) {
        dc_gpu_t *g=grid();ASSERT_TRUE(g);
        dc_catapult_scene_t scene={0};
        ASSERT_TRUE(dc_catapult_scene_build(g,false,&scene,err,sizeof(err)));
        float scale=cadence==1u?2.f:cadence==2u?.5f:1.f;
        ASSERT_TRUE(dc_gpu_set_tick_seconds(g,scale/60.f));
        uint32_t held=cadence==1u?10u:cadence==2u?40u:20u;
        uint32_t flight=held;
        for(uint32_t tick=0;tick<held;tick++)
            ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
        ASSERT_TRUE(dc_catapult_scene_release(g,&scene,err,sizeof(err)));
        for(uint32_t tick=0;tick<flight;tick++)
            ASSERT_TRUE(dc_gpu_tick_step(g,err,sizeof(err)));
        dc_gpu_world_body_t coal;dc_gpu_body_motion_t arm;
        fr_gas_gpu_clock_t gas;
        ASSERT_TRUE(dc_gpu_read_world_body(g,scene.coal_body,&coal,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(g,scene.arm_body,&arm,err,sizeof(err)));
        ASSERT_TRUE(fr_gas_gpu_clock(g,&gas));
        position[cadence][0]=xof(coal);position[cadence][1]=yof(coal);
        angle[cadence]=arm.angle;
        ASSERT_TRUE(fabs(gas.advanced_seconds-2./3.)<1e-4);
        dc_gpu_destroy(g);
    }
    for(uint32_t cadence=1;cadence<3;cadence++) {
        float separation=hypotf(position[0][0]-position[cadence][0],
                                 position[0][1]-position[cadence][1]);
        printf("elapsed launcher cadence %u separation %.3f, arm angles %.3f %.3f\n",
            cadence,separation,angle[0],angle[cadence]);
        ASSERT_TRUE(separation<2.f&&fabsf(angle[0]-angle[cadence])<.2f);
    }
    PASS();
}

#ifndef DC_CATAPULT_DISPLAY_INCLUDE
int main(void) {
    RUN(test_grounded_triangular_frame_and_tension_release);
    RUN(test_cold_launcher_has_supported_frame_and_real_latch);
    RUN(test_hot_coal_load_keeps_finite_fuel_and_checkpointed_joint);
    RUN(test_hot_coal_emits_finite_reaction_without_igniting_cold_control);
    RUN(test_wooden_arm_has_finite_reactive_ownership);
    RUN(test_prompt_hot_launch_keeps_sound_wood_arm_unignited);
    RUN(test_equal_elapsed_cadences_keep_launcher_flight_and_gas_clock);
    printf("%d passed, %d failed\n",g_pass,g_fail);
    return g_fail?1:0;
}
#endif
