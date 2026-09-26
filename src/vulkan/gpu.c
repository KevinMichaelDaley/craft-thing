#include "dungeoncraft/gpu.h"

bool dc_gpu_create(dc_gpu_t **out, uint32_t width, uint32_t height,
                   const char *shader_path, char *err_buf, uint32_t err_cap) {
    (void)out; (void)width; (void)height; (void)shader_path;
    (void)err_buf; (void)err_cap;
    return false;
}

bool dc_gpu_pattern(dc_gpu_t *gpu, char *err_buf, uint32_t err_cap) {
    (void)gpu; (void)err_buf; (void)err_cap;
    return false;
}

bool dc_gpu_readback(dc_gpu_t *gpu, uint32_t *cells, uint32_t cell_count,
                     char *err_buf, uint32_t err_cap) {
    (void)gpu; (void)cells; (void)cell_count; (void)err_buf; (void)err_cap;
    return false;
}

void dc_gpu_destroy(dc_gpu_t *gpu) { (void)gpu; }
