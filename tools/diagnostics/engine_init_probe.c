#include <stdio.h>

#include "dungeoncraft/gpu.h"

extern int fr_diag_forbid_submit;
int main(void) {
    dc_gpu_t *gpu = NULL;
    char err[256] = {0};
    setvbuf(stdout, NULL, _IOLBF, 0);
    fr_diag_forbid_submit = 1;
    puts("engine64x64 initialization begin; GPU queue submissions forbidden");
    if (!dc_gpu_create(&gpu, 64, 64, "build/shaders/pattern.comp.spv", err,
                       sizeof(err))) {
        printf("engine initialization failed: %s\n", err);
        return 1;
    }
    puts("engine initialization complete; destroying idle context");
    dc_gpu_destroy(gpu);
    puts("PASS engine initialization without GPU submission");
    return 0;
}
