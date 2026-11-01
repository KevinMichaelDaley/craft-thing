#include "ferrum/player/drive_gpu.h"

bool fr_player_drive_gpu_create(dc_gpu_t *gpu,const fr_player_body_binding_t *bindings,
    uint32_t count,float speed,float jump,fr_player_drive_gpu_t **out,char *err,uint32_t cap) {
    (void)gpu;(void)bindings;(void)count;(void)speed;(void)jump;(void)out;(void)err;(void)cap;
    return false;
}
bool fr_player_drive_gpu_submit(fr_player_drive_gpu_t *drive,
    const fr_player_drive_input_t *inputs,uint32_t count,char *err,uint32_t cap) {
    (void)drive;(void)inputs;(void)count;(void)err;(void)cap;
    return false;
}
void fr_player_drive_gpu_destroy(fr_player_drive_gpu_t *drive) {(void)drive;}
