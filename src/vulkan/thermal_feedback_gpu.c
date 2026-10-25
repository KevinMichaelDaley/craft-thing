#include "thermal_feedback_gpu.h"

bool fr_thermal_feedback_init(dc_gpu_t *g,fr_renderer_compute_t *out,
    VkDescriptorBufferInfo samples,VkDescriptorBufferInfo probe,char *err,uint32_t cap) {
    VkDescriptorBufferInfo buffers[3]={samples,{g->cells,0,VK_WHOLE_SIZE},probe};
    return fr_renderer_compute_init(g,out,buffers,3,32,"build/shaders/thermal_overlay.comp.spv",err,cap);
}
void fr_thermal_feedback_record(dc_gpu_t *g,const fr_renderer_compute_t *feedback,
    uint32_t mode,uint32_t groups_x,uint32_t groups_y) {
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    uint32_t push[8]={g->width,g->height,g->view_x,g->view_y,mode,0,0,0};
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,feedback->pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,feedback->pipeline_layout,0,1,&feedback->descriptor,0,NULL);
    vkCmdPushConstants(g->command,feedback->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,groups_x,groups_y,1);
}
