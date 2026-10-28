#define main existing_rigid_solver_main
#include "rigid_solver_tests.c"
#undef main
#include "ferrum/physics/revolute_gpu.h"

static void test_pivot_keeps_anchors_together_while_arm_rotates(void) {
    dc_gpu_t *g = grid(); char err[256]; ASSERT_TRUE(g);
    ASSERT_TRUE(dc_gpu_set_rigid_solver(g, true));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g, box(1, 60, 40, 4, 4), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_spawn_world_body(g, box(2, 62, 46, 2, 12), err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g, 1, 0, 0, DC_GPU_BODY_KINEMATIC, err, sizeof(err)));
    ASSERT_TRUE(dc_gpu_set_body_motion(g, 2, 0, .03f, 0, err, sizeof(err)));
    fr_revolute_joint_t joint = {.id=1,.body_a=1,.body_b=2,
        .anchor_a={0,2},.anchor_b={-1,-8}};
    ASSERT_TRUE(fr_gpu_revolute_add(g, &joint, err, sizeof(err)));
    float maximum_error=0, maximum_rotation=0;
    for (unsigned tick=0;tick<60;tick++) {
        ASSERT_TRUE(dc_gpu_rigid_step(g, err, sizeof(err)));
        dc_gpu_world_body_t arm; dc_gpu_body_motion_t motion;
        ASSERT_TRUE(dc_gpu_read_world_body(g, 2, &arm, err, sizeof(err)));
        ASSERT_TRUE(dc_gpu_read_body_motion(g, 2, &motion, err, sizeof(err)));
        float c=cosf(motion.angle),s=sinf(motion.angle);
        float x=xof(arm)+motion.center_x-c+8*s;
        float y=yof(arm)+motion.center_y-s-8*c;
        maximum_error=fmaxf(maximum_error,hypotf(x-62,y-44));
        maximum_rotation=fmaxf(maximum_rotation,fabsf(motion.angle));
    }
    printf("pivot anchor error %.6f, arm rotation %.6f\n",maximum_error,maximum_rotation);
    ASSERT_TRUE(maximum_error<.1f); ASSERT_TRUE(maximum_rotation>.02f);
    ASSERT_TRUE(fr_gpu_revolute_remove(g, 1, err, sizeof(err)));
    dc_gpu_destroy(g); PASS();
}
int main(void) {
    RUN(test_pivot_keeps_anchors_together_while_arm_rotates);
    printf("%d passed, %d failed\n",g_pass,g_fail); return g_fail?1:0;
}
