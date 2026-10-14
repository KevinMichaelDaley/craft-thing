#include <stdio.h>

#include "ferrum/structural/authoring.h"
#include "gpu_internal.h"

bool fr_gpu_prepare_authored(dc_gpu_t *gpu, const fr_authored_request_t *request,
                             char *err_buf, uint32_t err_cap) {
    (void)gpu;
    (void)request;
    if (err_buf && err_cap)
        snprintf(err_buf, err_cap, "Authored GPU staging is not implemented");
    return false;
}

bool fr_gpu_read_authored_stage(dc_gpu_t *gpu,
                                fr_authored_stage_diagnostic_t *diagnostic,
                                uint32_t *labels, uint32_t label_capacity,
                                char *err_buf, uint32_t err_cap) {
    (void)gpu; (void)diagnostic; (void)labels; (void)label_capacity;
    (void)err_buf; (void)err_cap;
    return false;
}
