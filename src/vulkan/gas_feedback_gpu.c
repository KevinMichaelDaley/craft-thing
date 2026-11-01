#include "gas_feedback_gpu.h"

bool fr_gas_feedback_init(dc_gpu_t *g,fr_renderer_compute_t *out,fr_renderer_compute_t *coordinates,
    fr_renderer_compute_t *potential,
    VkDescriptorBufferInfo world,VkDescriptorBufferInfo alternate,VkDescriptorBufferInfo model,VkDescriptorBufferInfo gates,
    VkDescriptorBufferInfo coordinates0,VkDescriptorBufferInfo coordinates1,
    VkDescriptorBufferInfo potential_buffer,char *err,uint32_t cap) {
    VkDescriptorBufferInfo buffers[9]={world,model,{g->page_buffer,0,VK_WHOLE_SIZE},{g->cells,0,VK_WHOLE_SIZE},
        gates,alternate,coordinates0,coordinates1,potential_buffer};
    return fr_renderer_compute_init(g,coordinates,buffers,9,36,"build/shaders/gas_coordinate.comp.spv",err,cap)&&
        fr_renderer_compute_init(g,potential,buffers,9,36,"build/shaders/gas_potential.comp.spv",err,cap)&&
        fr_renderer_compute_init(g,out,buffers,9,36,"build/shaders/gas_overlay.comp.spv",err,cap);
}
void fr_gas_feedback_record(dc_gpu_t *g,const fr_renderer_compute_t *feedback,
    const fr_renderer_compute_t *coordinates,const fr_renderer_compute_t *potential,
    uint32_t gx,uint32_t gy,float dt,uint32_t bank,bool reset) {
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    uint32_t push[9]={g->width,g->height,g->view_x,g->view_y,g->view_width,g->view_height,0,bank,reset?1u:0u};
    memcpy(&push[6],&dt,sizeof(dt));
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,coordinates->pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,coordinates->pipeline_layout,0,1,&coordinates->descriptor,0,NULL);
    vkCmdPushConstants(g->command,coordinates->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,gx,gy,1);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    push[6]=0u;push[7]=bank^1u;push[8]=0u;
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,potential->pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,potential->pipeline_layout,0,1,&potential->descriptor,0,NULL);
    vkCmdPushConstants(g->command,potential->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,gx,gy,1);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,feedback->pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,feedback->pipeline_layout,0,1,&feedback->descriptor,0,NULL);
    vkCmdPushConstants(g->command,feedback->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,gx,gy,1);
}
