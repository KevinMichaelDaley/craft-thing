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
int main(void) {
    RUN(test_pivot_keeps_anchors_together_while_arm_rotates);
    RUN(test_reused_body_identity_cannot_rebind_old_pivot);
    RUN(test_finite_spring_drives_arm_without_inventing_energy);
    printf("%d passed, %d failed\n",g_pass,g_fail); return g_fail?1:0;
}
