#include <stdio.h>
#include "gpu_internal.h"

bool dc_gpu_solver_commands_init(dc_gpu_t *gpu,char *err,uint32_t cap) {
    (void)gpu;if(err&&cap) snprintf(err,cap,"Solver command caching is not implemented");
    return false;
}
void dc_gpu_solver_commands_destroy(dc_gpu_t *gpu) {(void)gpu;}
bool dc_gpu_solver_commands_stats(const dc_gpu_t *gpu,uint64_t *recordings,uint64_t *executions) {
    (void)gpu;(void)recordings;(void)executions;return false;
}
