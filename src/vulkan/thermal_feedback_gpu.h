#ifndef FR_THERMAL_FEEDBACK_GPU_H
#define FR_THERMAL_FEEDBACK_GPU_H
#include "renderer_internal.h"
/** Bind borrowed simulation snapshots to a purely visual adapter. */
bool fr_thermal_feedback_init(dc_gpu_t *gpu,fr_renderer_compute_t *out,
    VkDescriptorBufferInfo samples,VkDescriptorBufferInfo probe,char *err,uint32_t cap);
/** Draw the current cache; never update an inventory or physical clock. */
void fr_thermal_feedback_record(dc_gpu_t *gpu,const fr_renderer_compute_t *feedback,
    uint32_t mode,uint32_t groups_x,uint32_t groups_y);
#endif
