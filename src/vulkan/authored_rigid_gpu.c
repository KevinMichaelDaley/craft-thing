#include <math.h>
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
static bool validate_objects(dc_gpu_t *g,const dc_gpu_address_buffer_t *channel,char *err,uint32_t cap) {
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
    return ((uint32_t *)channel->mapped)[2]==0u||
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
    if(!dc_gpu_address_buffer_create(g,((VkDeviceSize)object_cells+4u)*4u,true,&channel,err,cap)) return false;
    memcpy((uint32_t *)channel.mapped+4u,objects,(size_t)object_cells*4u);
    g->authored_objects_address=channel.address;g->authored_objects_transaction=transaction;
    fr_gpu_authored_objects_bind(g,g->runtime_bodies->work.mapped);g->descriptor_revision++;
    bool okay=validate_objects(g,&channel,err,cap);
    while(okay&&((uint32_t *)channel.mapped)[1]) {
        uint32_t previous=((uint32_t *)channel.mapped)[1],requests[16];
        uint64_t revision=g->descriptor_revision;
        memcpy(requests,g->runtime_bodies->mailbox.mapped,sizeof(requests));
        okay=dc_gpu_admit_authored_rigid_batch(g,err,cap);
        if(okay&&previous==((uint32_t *)channel.mapped)[1]&&revision==g->descriptor_revision&&
           memcmp(requests,g->runtime_bodies->mailbox.mapped,sizeof(requests))==0)
            okay=error(err,cap,"Authored object admission made no progress and has no resource demand");
    }
    bool detached=detach_objects(g,okay?err:NULL,okay?cap:0u);
    dc_gpu_address_buffer_destroy(g,&channel);
    return okay&&detached;
}
bool fr_gpu_pose_authored_rigid(dc_gpu_t *g,uint32_t body_id,dc_chunk_coord_t chunk,
    int32_t x_fp,int32_t y_fp,float angle,char *err,uint32_t cap) {
    if(!g||!g->runtime_bodies||!body_id||x_fp<0||y_fp<0||x_fp>=64*65536||y_fp>=64*65536||
       !isfinite(angle)||fabsf(angle)>1000000.f)
        return error(err,cap,"Invalid initial authored rigid transform");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot reach authored transform boundary");
    for(uint32_t slot=0;slot<dc_gpu_body_capacity(g);slot++) {
        dc_gpu_body_record_t *b=dc_gpu_body_record(g,slot);
        dc_body_storage_owner_t *owner=dc_gpu_body_owner(g,slot);
        if(!b->body.active||b->body.id!=body_id||owner->state!=DC_BODY_OWNER_LIVE||
           owner->runtime_id!=body_id||!owner->source_object||
           (!owner->source_transaction.low&&!owner->source_transaction.high)) continue;
        dc_gpu_body_record_t previous=*b;
        uint64_t x=(uint64_t)chunk.x,y=(uint64_t)chunk.y;
        b->chunk_x[0]=(uint32_t)x;b->chunk_x[1]=(uint32_t)(x>>32u);
        b->chunk_y[0]=(uint32_t)y;b->chunk_y[1]=(uint32_t)(y>>32u);
        b->local_x_fp=x_fp;b->local_y_fp=y_fp;
        b->angle=remainderf(angle,6.283185307179586f);b->rest_ticks=0;
        g->body_refresh_pending=true;
        if(dc_gpu_set_body_origin(g,g->body_origin,err,cap)) return true;
        *dc_gpu_body_record(g,slot)=previous;g->body_refresh_pending=true;return false;
    }
    return error(err,cap,"Transform target is not a live explicitly authored rigid object");
}
