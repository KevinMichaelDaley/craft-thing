#ifndef DC_BODY_STORAGE_GPU_H
#define DC_BODY_STORAGE_GPU_H
#include "body_address_gpu.h"
#include "dungeoncraft/body_storage_layout.h"

typedef struct dc_gpu_body_block dc_gpu_body_block_t;
typedef struct {
    dc_gpu_address_buffer_t directory;
    dc_gpu_body_block_t *blocks;
    uint32_t slab_count;
} dc_gpu_body_store_t;
typedef struct {
    dc_gpu_address_buffer_t directory;
    dc_gpu_body_block_t *block;
    uint32_t previous_slabs, slab_count;
    uint64_t submission;
} dc_gpu_body_growth_t;

/** Prepare resources without altering active state or copying body payloads on CPU. */
bool dc_gpu_body_growth_prepare(dc_gpu_t *gpu,const dc_gpu_body_store_t *store,
    uint32_t required_slots,dc_gpu_body_growth_t *growth,char *err,uint32_t cap);
/** Record parallel page construction, block clearing and publication at an idle boundary. */
bool dc_gpu_body_growth_record(VkCommandBuffer command,VkBuffer root,VkPipelineLayout layout,VkPipeline initialize,
    const dc_gpu_body_store_t *store,dc_gpu_body_growth_t *growth,uint64_t submission);
/** Adopt resources only after the recorded submission completed; retire its old directory. */
bool dc_gpu_body_growth_finish(dc_gpu_t *gpu,dc_gpu_body_store_t *store,
    dc_gpu_body_growth_t *growth,uint64_t completed_submission);
/** Release an unsubmitted preparation. Submitted resources require completion first. */
bool dc_gpu_body_growth_discard(dc_gpu_t *gpu,dc_gpu_body_growth_t *growth);
/** Destroy the store after all uses have completed. */
void dc_gpu_body_store_destroy(dc_gpu_t *gpu,dc_gpu_body_store_t *store);
#endif
