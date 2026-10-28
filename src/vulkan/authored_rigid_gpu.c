#include <stdio.h>
#include <string.h>

#include "ferrum/physics/authored_rigid_gpu.h"
#include "authored_rigid_gpu_internal.h"
#include "runtime_body_internal.h"

static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
void fr_gpu_authored_objects_bind(dc_gpu_t *g,uint32_t *h) {
    h[104]=(uint32_t)g->authored_objects_address;h[105]=(uint32_t)(g->authored_objects_address>>32u);
    h[106]=(uint32_t)g->authored_objects_transaction;h[107]=(uint32_t)(g->authored_objects_transaction>>32u);
    h[108]=0;
}
static bool validate_objects(dc_gpu_t *g,char *err,uint32_t cap) {
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin rigid-object validation");
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->pipeline_layout,0,1,&g->descriptor,0,NULL);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,g->component_object_pipelines[38]);
    uint64_t x=(uint64_t)g->body_origin.x,y=(uint64_t)g->body_origin.y;
    uint32_t push[]={g->width,g->height,38u,0u,(uint32_t)x,(uint32_t)(x>>32u),(uint32_t)y,(uint32_t)(y>>32u)};
    vkCmdPushConstants(g->command,g->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdDispatch(g->command,(DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS+127u)/128u,1,1);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    if(!fr_renderer_submit(g)) return error(err,cap,"Cannot validate rigid-object ownership");
    return ((uint32_t *)g->runtime_bodies->work.mapped)[108]==0u||
        error(err,cap,"Authored rigid object selects terrain, absent, fluid or loose material");
}
static bool detach_objects(dc_gpu_t *g,char *err,uint32_t cap) {
    g->authored_objects_address=0;g->authored_objects_transaction=0;
    fr_gpu_authored_objects_bind(g,g->runtime_bodies->work.mapped);g->descriptor_revision++;
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot reset cold object connectivity cache");
    vkCmdFillBuffer(g->command,g->runtime_bodies->component_cache.buffer,0,VK_WHOLE_SIZE,0);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    return fr_renderer_submit(g)||error(err,cap,"Cannot publish ordinary connectivity after object admission");
}
bool fr_gpu_admit_rigid_objects(dc_gpu_t *g,uint64_t transaction,
    const uint32_t *objects,uint32_t object_cells,char *err,uint32_t cap) {
    if(!g||!g->runtime_bodies||!g->runtime_body_consumers||!transaction||!objects||
       object_cells!=DC_GPU_CHUNK_SLOTS*DC_CHUNK_CELLS||g->authored_objects_address)
        return error(err,cap,"Invalid immutable rigid-object submission");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot reach rigid-object admission boundary");
    if(!dc_gpu_component_objects_prepare(g,err,cap)) return false;
    dc_gpu_address_buffer_t channel={0};
    if(!dc_gpu_address_buffer_create(g,(VkDeviceSize)object_cells*4u,true,&channel,err,cap)) return false;
    memcpy(channel.mapped,objects,(size_t)object_cells*4u);
    g->authored_objects_address=channel.address;g->authored_objects_transaction=transaction;
    fr_gpu_authored_objects_bind(g,g->runtime_bodies->work.mapped);g->descriptor_revision++;
    bool okay=validate_objects(g,err,cap)&&dc_gpu_admit_authored_rigid_cells(g,err,cap);
    bool detached=detach_objects(g,okay?err:NULL,okay?cap:0u);
    dc_gpu_address_buffer_destroy(g,&channel);
    return okay&&detached;
}
