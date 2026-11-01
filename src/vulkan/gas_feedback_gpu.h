#ifndef FR_GAS_FEEDBACK_GPU_H
#define FR_GAS_FEEDBACK_GPU_H
#include "renderer_internal.h"
/** Bind immutable gas state and model parameters to the visual adapter. */
bool fr_gas_feedback_init(dc_gpu_t *gpu,fr_renderer_compute_t *out,fr_renderer_compute_t *coordinates,
    fr_renderer_compute_t *potential,
    VkDescriptorBufferInfo world,VkDescriptorBufferInfo alternate,VkDescriptorBufferInfo model,VkDescriptorBufferInfo gates,
    VkDescriptorBufferInfo coordinates0,VkDescriptorBufferInfo coordinates1,
    VkDescriptorBufferInfo potential_buffer,char *err,uint32_t cap);
/** Composite the current gas field without changing its inventories. */
void fr_gas_feedback_record(dc_gpu_t *gpu,const fr_renderer_compute_t *feedback,
    const fr_renderer_compute_t *coordinates,const fr_renderer_compute_t *potential,
    uint32_t groups_x,uint32_t groups_y,float dt,uint32_t bank,bool reset);
#endif
