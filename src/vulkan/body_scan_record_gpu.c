#include "body_scan_gpu.h"
bool dc_gpu_body_scan_record(VkCommandBuffer command,const dc_gpu_body_scan_plan_t *plan,
    VkDeviceAddress address,VkDeviceSize bytes,VkPipelineLayout layout,VkPipeline scan,VkPipeline add) {
    (void)command;(void)plan;(void)address;(void)bytes;(void)layout;(void)scan;(void)add;return false;
}
