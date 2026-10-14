#ifndef FERRUM_RENDERER_INTERNAL_H
#define FERRUM_RENDERER_INTERNAL_H

#include "gpu_internal.h"

typedef struct {
  VkDescriptorSetLayout layout;
  VkDescriptorPool pool;
  VkDescriptorSet descriptor;
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
} fr_renderer_compute_t;

bool fr_renderer_compute_init(dc_gpu_t *gpu, fr_renderer_compute_t *compute,
                              const VkDescriptorBufferInfo *buffers,
                              uint32_t count, uint32_t push_bytes,
                              const char *shader, char *err, uint32_t cap);
void fr_renderer_compute_destroy(dc_gpu_t *gpu, fr_renderer_compute_t *compute);
void fr_renderer_barrier(VkCommandBuffer command, VkPipelineStageFlags2 source,
                         VkAccessFlags2 source_access,
                         VkPipelineStageFlags2 destination,
                         VkAccessFlags2 destination_access);
bool fr_renderer_begin(dc_gpu_t *gpu);
bool fr_renderer_submit(dc_gpu_t *gpu);

#endif
