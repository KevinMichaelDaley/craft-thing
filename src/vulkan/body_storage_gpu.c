#include "body_storage_gpu.h"
bool dc_gpu_body_growth_prepare(dc_gpu_t *gpu,const dc_gpu_body_store_t *store,
    uint32_t required_slots,dc_gpu_body_growth_t *growth,char *err,uint32_t cap) {
    (void)gpu;(void)store;(void)required_slots;(void)growth;(void)err;(void)cap;return false;
}
bool dc_gpu_body_growth_record(VkCommandBuffer command,VkBuffer root,
    const dc_gpu_body_store_t *store,dc_gpu_body_growth_t *growth,uint64_t submission) {
    (void)command;(void)root;(void)store;(void)growth;(void)submission;return false;
}
bool dc_gpu_body_growth_finish(dc_gpu_t *gpu,dc_gpu_body_store_t *store,
    dc_gpu_body_growth_t *growth,uint64_t completed_submission) {
    (void)gpu;(void)store;(void)growth;(void)completed_submission;return false;
}
bool dc_gpu_body_growth_discard(dc_gpu_t *gpu,dc_gpu_body_growth_t *growth) {(void)gpu;(void)growth;return true;}
void dc_gpu_body_store_destroy(dc_gpu_t *gpu,dc_gpu_body_store_t *store) {(void)gpu;(void)store;}
