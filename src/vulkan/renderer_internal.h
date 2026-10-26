#ifndef FERRUM_RENDERER_INTERNAL_H
#define FERRUM_RENDERER_INTERNAL_H

#include "ferrum/renderer/lighting_gpu.h"
#include "ferrum/renderer/occlusion_gpu.h"
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
/** Create a compute pipeline with a previously validated subgroup width;
 * zero retains driver selection. Callers must enable subgroupSizeControl. */
bool fr_renderer_compute_init_subgroup(dc_gpu_t *gpu,fr_renderer_compute_t *compute,
    const VkDescriptorBufferInfo *buffers,uint32_t count,uint32_t push_bytes,
    const char *shader,uint32_t subgroup,char *err,uint32_t cap);
void fr_renderer_compute_destroy(dc_gpu_t *gpu, fr_renderer_compute_t *compute);
void fr_renderer_barrier(VkCommandBuffer command, VkPipelineStageFlags2 source,
                         VkAccessFlags2 source_access,
                         VkPipelineStageFlags2 destination,
                         VkAccessFlags2 destination_access);
bool fr_renderer_begin(dc_gpu_t *gpu);
bool fr_renderer_submit(dc_gpu_t *gpu);
bool fr_occlusion_gpu_layout(fr_occlusion_gpu_t *occlusion,
                             VkDescriptorBufferInfo *buffer, uint32_t *layout);

bool fr_occlusion_input_binding(fr_occlusion_gpu_t *occlusion,
                                VkDescriptorBufferInfo *buffer);
bool fr_occlusion_input_read(fr_occlusion_gpu_t *occlusion,uint32_t x,uint32_t y,
                              fr_occlusion_node_t *node);
void fr_occlusion_record(fr_occlusion_gpu_t *occlusion, VkBuffer indirect,
                         VkDeviceSize offset);
bool fr_lighting_field_binding(fr_lighting_gpu_t *lighting,
                               VkDescriptorBufferInfo *buffer);
bool fr_renderer_authored_bindings(dc_gpu_t *gpu, VkDescriptorBufferInfo buffers[3]);
void fr_lighting_record(fr_lighting_gpu_t *lighting, VkBuffer indirect,
                        VkDeviceSize offset);
/** Rebind appearance after a completed resource-growth transaction. */
void fr_renderer_rebind_colors(dc_gpu_t *gpu);

#endif
