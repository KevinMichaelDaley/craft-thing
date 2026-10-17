#include "body_address_gpu.h"
bool dc_gpu_address_buffer_create(dc_gpu_t *gpu,VkDeviceSize bytes,bool host_visible,
    dc_gpu_address_buffer_t *out,char *err,uint32_t cap) {
    (void)gpu;(void)bytes;(void)host_visible;(void)out;(void)err;(void)cap;return false;
}
void dc_gpu_address_buffer_destroy(dc_gpu_t *gpu,dc_gpu_address_buffer_t *buffer) {(void)gpu;(void)buffer;}
