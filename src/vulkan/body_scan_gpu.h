#ifndef DC_BODY_SCAN_GPU_H
#define DC_BODY_SCAN_GPU_H
#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>

enum {DC_BODY_SCAN_LANES=128};
typedef struct {
    uint32_t count,groups,groups_x,groups_y;
    VkDeviceSize offset;
} dc_gpu_body_scan_level_t;
typedef struct {
    dc_gpu_body_scan_level_t *levels;
    uint32_t level_count;
    VkDeviceSize total_offset,bytes;
} dc_gpu_body_scan_plan_t;
/** Plan hierarchical GPU scans at resource growth; does not read or solve bodies. */
bool dc_gpu_body_scan_plan_create(uint32_t count,uint32_t max_groups_x,uint32_t max_groups_y,
    dc_gpu_body_scan_plan_t *out,char *err,uint32_t cap);
/** Release host-owned dispatch metadata after recorded commands no longer need it. */
void dc_gpu_body_scan_plan_destroy(dc_gpu_body_scan_plan_t *plan);
#endif
