#include "catapult_scene.h"

bool dc_catapult_scene_build(dc_gpu_t *gpu, bool hot,
                             dc_catapult_scene_t *scene,
                             char *err_buf, uint32_t err_cap) {
    (void)gpu;(void)hot;(void)scene;(void)err_buf;(void)err_cap;
    return false;
}

bool dc_catapult_scene_release(dc_gpu_t *gpu, dc_catapult_scene_t *scene,
                               char *err_buf, uint32_t err_cap) {
    (void)gpu;(void)scene;(void)err_buf;(void)err_cap;
    return false;
}
