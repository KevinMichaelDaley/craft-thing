#include "dungeoncraft/gpu.h"
#ifdef FR_REVOLUTE_PRESENTATION_TEST
#include <SDL.h>
static bool pivot_window(dc_gpu_t **g,uint32_t w,uint32_t h,const char *shader,char *err,uint32_t cap) {
    return dc_gpu_create_window(g,w,h,1280,720,shader,err,cap)&&
        dc_gpu_set_presentation_focus(*g,(dc_presentation_focus_t){42,30,54,42});
}
#define dc_gpu_create pivot_window
#endif
#define main existing_rigid_solver_main
#include "rigid_solver_tests.c"
#undef main
#ifdef FR_REVOLUTE_PRESENTATION_TEST
#undef dc_gpu_create
#endif
#include "ferrum/physics/revolute_gpu.h"
#include "ferrum/structural/gpu.h"
#include "../../src/vulkan/runtime_body_gpu.h"

static void test_pivot_keeps_anchors_together_while_arm_rotates(void) {
    dc_gpu_t *g = grid(); char err[256]; ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g, true));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g, box(1, 60, 40, 4, 4), err, sizeof(err)));
    dc_gpu_body_shape_t wood=rectangle(DC_MATERIAL_WOOD,2,12);
    ASSERT_TRUE(dc_gpu_spawn_convex_body(g, box(2, 62, 46, 2, 12), &wood, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g, 1, 0, 0, DC_GPU_BODY_KINEMATIC, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g, 2, 0, .03f, 0, err, sizeof(err)));
    fr_revolute_joint_t joint = {.id=1,.body_a=1,.body_b=2,
        .anchor_a={0,2},.anchor_b={-1,-8}};
    ASSERT_TRUE(fr_gpu_revolute_add(g, &joint, err, sizeof(err)));
    float maximum_error=0, maximum_rotation=0;
    for (unsigned tick=0;tick<180;tick++) {
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
        dc_gpu_world_body_t arm; dc_gpu_body_motion_t motion;
        ASSERT_TRUE(dc_gpu_read_world_body(g, 2, &arm, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(g, 2, &motion, err, sizeof(err)));
        float c=cosf(motion.angle),s=sinf(motion.angle);
        float x=xof(arm)+motion.center_x-c+8*s;
        float y=yof(arm)+motion.center_y-s-8*c;
        maximum_error=fmaxf(maximum_error,hypotf(x-62,y-44));
        maximum_rotation=fmaxf(maximum_rotation,fabsf(motion.angle));
#ifdef FR_REVOLUTE_PRESENTATION_TEST
        ASSERT_TRUE(dc_gpu_present_chunks_steps(g,0,err,sizeof(err)));
        SDL_Event event;while(SDL_PollEvent(&event)) ASSERT_TRUE(event.type!=SDL_QUIT);
        SDL_Delay(16);
#endif
    }
    printf("pivot anchor error %.6f, arm rotation %.6f\n",maximum_error,maximum_rotation);
    ASSERT_TRUE(maximum_error<.1f); ASSERT_TRUE(maximum_rotation>.02f);
    ASSERT_TRUE(fr_gpu_revolute_remove(g, 1, err, sizeof(err)));
    dc_gpu_destroy(g); PASS();
}
static void test_reused_body_identity_cannot_rebind_old_pivot(void) {
    dc_gpu_t *g=grid();char err[256];ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,box(1,60,40,4,4),err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,box(2,62,46,2,12),err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,1,0,0,DC_GPU_BODY_KINEMATIC,err,sizeof(err)));
    fr_revolute_joint_t joint={.id=1,.body_a=1,.body_b=2,.anchor_a={0,2},.anchor_b={-1,-8}};
    ASSERT_TRUE(fr_gpu_revolute_add(g,&joint,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_remove_body(g,2,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,box(2,80,30,3,3),err,sizeof(err)));
    for(unsigned tick=0;tick<5;tick++) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_world_body_t body;ASSERT_TRUE(dc_gpu_read_world_body(g,2,&body,err,sizeof(err)));
    ASSERT_TRUE(xof(body)>79.9f&&yof(body)>30.5f);
    dc_gpu_destroy(g);PASS();
}
static void test_finite_spring_drives_arm_without_inventing_energy(void) {
    dc_gpu_t *g=grid();char err[256];ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,box(1,60,20,4,4),err,sizeof(err)));
    dc_gpu_body_shape_t wood=rectangle(DC_MATERIAL_WOOD,2,12);
    ASSERT_TRUE(dc_gpu_spawn_convex_body(g,box(2,61,36,2,12),&wood,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,1,0,0,DC_GPU_BODY_KINEMATIC,err,sizeof(err)));
    fr_revolute_joint_t joint={.id=1,.body_a=1,.body_b=2,.anchor_a={0,20},.anchor_b={0,0},
        .flags=FR_REVOLUTE_SPRING,.spring_stiffness=1,.spring_rest_angle=.4f};
    ASSERT_TRUE(fr_gpu_revolute_add(g,&joint,err,sizeof(err)));
    float peak_energy=0,rotation=0;
    for(unsigned tick=0;tick<90;tick++) {
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
        dc_gpu_body_motion_t m;ASSERT_TRUE(dc_gpu_read_body_motion(g,2,&m,err,sizeof(err)));
        float error=m.angle-.4f,energy=.5f*error*error+.5f*m.inertia*m.angular_velocity*m.angular_velocity;
        peak_energy=fmaxf(peak_energy,energy);rotation=fmaxf(rotation,m.angle);
    }
    printf("spring initial energy .08, peak %.6f, rotation %.6f\n",peak_energy,rotation);
    ASSERT_TRUE(rotation>.1f);ASSERT_TRUE(peak_energy<.082f);
    dc_gpu_destroy(g);PASS();
}
static void test_checkpoint_restores_joint_and_remaining_spring_work(void) {
    dc_gpu_t *g=grid();char err[256];ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,box(1,60,20,4,4),err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,box(2,61,36,2,12),err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,1,0,0,DC_GPU_BODY_KINEMATIC,err,sizeof(err)));
    fr_revolute_joint_t joint={.id=1,.body_a=1,.body_b=2,.anchor_a={0,20},
        .flags=FR_REVOLUTE_SPRING,.spring_stiffness=1,.spring_rest_angle=.4f};
    ASSERT_TRUE(fr_gpu_revolute_add(g,&joint,err,sizeof(err)));
    for(unsigned tick=0;tick<5;tick++) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    ASSERT_TRUE(fr_gpu_save_structure(g,"build/revolute_checkpoint.dcs",err,sizeof(err)));
    dc_gpu_t *restored=grid();ASSERT_TRUE(restored);
    ASSERT_TRUE(fr_gpu_load_structure(restored,"build/revolute_checkpoint.dcs",err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_rigid_solver(restored,true));
    for(unsigned tick=0;tick<20;tick++) {
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
        ASSERT_TRUE(dc_gpu_rigid_step(restored,err,sizeof(err)));
    }
    dc_gpu_body_motion_t a,b;
    ASSERT_TRUE(dc_gpu_read_body_motion(g,2,&a,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(restored,2,&b,err,sizeof(err)));
    printf("restored spring arm angles %.6f %.6f\n",a.angle,b.angle);
    ASSERT_TRUE(fabsf(a.angle-b.angle)<.001f&&fabsf(a.angular_velocity-b.angular_velocity)<.001f);
    dc_gpu_destroy(g);dc_gpu_destroy(restored);PASS();
}
static void test_angular_limits_stop_rotation_and_overload_releases_anchor(void) {
    dc_gpu_t *g=grid();char err[256];ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,box(1,60,20,4,4),err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,box(2,61,36,2,12),err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,1,0,0,DC_GPU_BODY_KINEMATIC,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,2,0,.04f,0,err,sizeof(err)));
    fr_revolute_joint_t joint={.id=1,.body_a=1,.body_b=2,.anchor_a={0,20},
        .flags=FR_REVOLUTE_LIMITS,.lower_angle=-.1f,.upper_angle=.1f};
    ASSERT_TRUE(fr_gpu_revolute_add(g,&joint,err,sizeof(err)));
    float rotation=0;
    for(unsigned tick=0;tick<30;tick++) {
        ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
        dc_gpu_body_motion_t m;ASSERT_TRUE(dc_gpu_read_body_motion(g,2,&m,err,sizeof(err)));
        ASSERT_TRUE(fabsf(m.angle)<=.101f);rotation=fmaxf(rotation,m.angle);
    }
    ASSERT_TRUE(rotation>.09f);
    ASSERT_TRUE(fr_gpu_revolute_remove(g,1,err,sizeof(err)));
    joint.id=2;joint.flags=0;joint.max_force=.001f;
    ASSERT_TRUE(fr_gpu_revolute_add(g,&joint,err,sizeof(err)));
    for(unsigned tick=0;tick<15;tick++) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_world_body_t body;ASSERT_TRUE(dc_gpu_read_world_body(g,2,&body,err,sizeof(err)));
    ASSERT_TRUE(yof(body)>45.f);
    dc_gpu_destroy(g);PASS();
}
static void test_joint_growth_and_body_growth_preserve_existing_pivot(void) {
    dc_gpu_t *g=grid();char err[256];ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,box(1,60,20,4,4),err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,box(2,61,36,2,12),err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,1,0,0,DC_GPU_BODY_KINEMATIC,err,sizeof(err)));
    fr_revolute_joint_t joint={.body_a=1,.body_b=2,.anchor_a={0,20}};
    for(unsigned id=1;id<=69;id++) {joint.id=id;ASSERT_TRUE(fr_gpu_revolute_add(g,&joint,err,sizeof(err)));}
    for(unsigned id=2;id<=69;id++) ASSERT_TRUE(fr_gpu_revolute_remove(g,id,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_runtime_bodies_reserve(g,256,err,sizeof(err)));
    for(unsigned tick=0;tick<10;tick++) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_world_body_t before,after;
    ASSERT_TRUE(dc_gpu_read_world_body(g,2,&before,err,sizeof(err)));
    ASSERT_TRUE(fabsf(yof(before)-36.f)<.01f);
    ASSERT_TRUE(fr_gpu_revolute_remove(g,1,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g,2,&after,err,sizeof(err)));
    ASSERT_TRUE(memcmp(&before,&after,sizeof(before))==0);
    dc_gpu_destroy(g);PASS();
}
static void test_free_frame_receives_recoil_without_net_horizontal_impulse(void) {
    dc_gpu_t *g=grid();char err[256];ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,box(1,60,40,4,4),err,sizeof(err)));
    dc_gpu_body_shape_t wood=rectangle(DC_MATERIAL_WOOD,2,12);
    ASSERT_TRUE(dc_gpu_spawn_convex_body(g,box(2,62,46,2,12),&wood,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,2,0,.03f,0,err,sizeof(err)));
    fr_revolute_joint_t joint={.id=1,.body_a=1,.body_b=2,.anchor_a={0,2},.anchor_b={-1,-8}};
    ASSERT_TRUE(fr_gpu_revolute_add(g,&joint,err,sizeof(err)));
    for(unsigned tick=0;tick<10;tick++) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_world_body_t a,b;dc_gpu_body_motion_t ma,mb;
    ASSERT_TRUE(dc_gpu_read_world_body(g,1,&a,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g,2,&b,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g,1,&ma,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_body_motion(g,2,&mb,err,sizeof(err)));
    float va=a.body.vx_fp/65536.f,vb=b.body.vx_fp/65536.f;
    printf("pivot recoil frame %.6f arm %.6f momentum %.6f\n",va,vb,ma.mass*va+mb.mass*vb);
    ASSERT_TRUE(fabsf(va)>.001f);
    ASSERT_TRUE(fabsf(ma.mass*va+mb.mass*vb)<.001f*(ma.mass+mb.mass));
    dc_gpu_destroy(g);PASS();
}
static void test_hinge_can_overlap_its_frame_without_self_collision(void) {
    dc_gpu_t *g=grid();char err[256];ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g,true));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,box(1,60,40,4,4),err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g,box(2,61,36,2,12),err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,1,0,0,DC_GPU_BODY_KINEMATIC,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g,2,0,.03f,0,err,sizeof(err)));
    fr_revolute_joint_t joint={.id=1,.body_a=1,.body_b=2};
    ASSERT_TRUE(fr_gpu_revolute_add(g,&joint,err,sizeof(err)));
    for(unsigned tick=0;tick<20;tick++) ASSERT_TRUE(dc_gpu_rigid_step(g,err,sizeof(err)));
    dc_gpu_body_motion_t motion;dc_gpu_world_body_t body;
    ASSERT_TRUE(dc_gpu_read_body_motion(g,2,&motion,err,sizeof(err)));
    ASSERT_TRUE(dc_gpu_read_world_body(g,2,&body,err,sizeof(err)));
    printf("overlapping hinge rotation %.6f pose %.6f %.6f\n",motion.angle,xof(body),yof(body));
    ASSERT_TRUE(fabsf(motion.angle-.6f)<.002f);
    ASSERT_TRUE(hypotf(xof(body)-61.f,yof(body)-36.f)<.01f);
    dc_gpu_destroy(g);PASS();
}
int main(void) {
    RUN(test_pivot_keeps_anchors_together_while_arm_rotates);
    RUN(test_reused_body_identity_cannot_rebind_old_pivot);
    RUN(test_finite_spring_drives_arm_without_inventing_energy);
    RUN(test_checkpoint_restores_joint_and_remaining_spring_work);
    RUN(test_angular_limits_stop_rotation_and_overload_releases_anchor);
    RUN(test_joint_growth_and_body_growth_preserve_existing_pivot);
    RUN(test_free_frame_receives_recoil_without_net_horizontal_impulse);
    RUN(test_hinge_can_overlap_its_frame_without_self_collision);
    printf("%d passed, %d failed\n",g_pass,g_fail); return g_fail?1:0;
}
