#ifndef FR_GAS_FEEDBACK_GPU_H
#define FR_GAS_FEEDBACK_GPU_H
#include "renderer_internal.h"
/** Bind immutable gas state and model parameters to the visual adapter. */
bool fr_gas_feedback_init(dc_gpu_t *gpu,fr_renderer_compute_t *out,
    VkDescriptorBufferInfo world,VkDescriptorBufferInfo model,char *err,uint32_t cap);
/** Composite the current gas field without changing its inventories. */
void fr_gas_feedback_record(dc_gpu_t *gpu,const fr_renderer_compute_t *feedback,
    uint32_t groups_x,uint32_t groups_y);
#endif
