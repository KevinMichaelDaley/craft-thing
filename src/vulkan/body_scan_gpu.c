#include "body_scan_gpu.h"
bool dc_gpu_body_scan_plan_create(uint32_t count,uint32_t max_groups_x,uint32_t max_groups_y,
    dc_gpu_body_scan_plan_t *out,char *err,uint32_t cap) {
    (void)count;(void)max_groups_x;(void)max_groups_y;(void)out;(void)err;(void)cap;return false;
}
void dc_gpu_body_scan_plan_destroy(dc_gpu_body_scan_plan_t *plan) {(void)plan;}
