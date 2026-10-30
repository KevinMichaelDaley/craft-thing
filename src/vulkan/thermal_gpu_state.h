#ifndef FR_THERMAL_GPU_STATE_H
#define FR_THERMAL_GPU_STATE_H
#include "body_address_gpu.h"
#include "renderer_internal.h"
struct fr_thermal_gpu {
    fr_renderer_compute_t compute,feedback,combustion,body_combustion;
    VkPipeline phase_pipelines[24];
    dc_gpu_address_buffer_t profiles,samples,fluxes,probe,water;
    dc_gpu_address_buffer_t emission,emission_snapshot,hud;
    bool hud_enabled;
    uint32_t groups_x,groups_y,overlay_groups_x,overlay_groups_y,profile_count,overlay;
    uint32_t purpose,pending_count;
    float *pending_water;
    bool probe_active;
};
#endif
