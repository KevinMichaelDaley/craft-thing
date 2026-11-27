#include "dungeoncraft/liquid_seam.h"

dc_liquid_seam_result_t dc_gpu_commit_liquid_seam(dc_liquid_seam_endpoint_t first,
    dc_liquid_seam_endpoint_t second, dc_liquid_seam_axis_t axis,
    const dc_liquid_seam_flux_t flux[DC_CHUNK_SIDE], char *err, uint32_t cap) {
    (void)first;
    (void)second;
    (void)axis;
    (void)flux;
    (void)err;
    (void)cap;
    return DC_LIQUID_SEAM_REJECTED;
}
