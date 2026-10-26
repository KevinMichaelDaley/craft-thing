#include "gas_feedback_gpu.h"

bool fr_gas_feedback_init(dc_gpu_t *g,fr_renderer_compute_t *out,
    VkDescriptorBufferInfo world,VkDescriptorBufferInfo alternate,VkDescriptorBufferInfo model,VkDescriptorBufferInfo gates,char *err,uint32_t cap) {
    VkDescriptorBufferInfo buffers[6]={world,model,{g->page_buffer,0,VK_WHOLE_SIZE},{g->cells,0,VK_WHOLE_SIZE},gates,alternate};
    return fr_renderer_compute_init(g,out,buffers,6,24,"build/shaders/gas_overlay.comp.spv",err,cap);
}
void fr_gas_feedback_record(dc_gpu_t *g,const fr_renderer_compute_t *feedback,uint32_t gx,uint32_t gy) {
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    uint32_t push[6]={g->width,g->height,g->view_x,g->view_y,g->view_width,g->view_height};
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,feedback->pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,feedback->pipeline_layout,0,1,&feedback->descriptor,0,NULL);
    vkCmdPushConstants(g->command,feedback->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,gx,gy,1);
}
