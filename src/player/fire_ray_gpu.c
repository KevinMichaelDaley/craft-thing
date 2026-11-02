#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/fire_ray_layout.h"
#include "ferrum/fire/thermal_gpu.h"
#include "ferrum/player/fire_ray_gpu.h"
#include "ferrum/renderer/gpu.h"
#include "../vulkan/fire_ray_renderer.h"
#include "../vulkan/gas_gpu_state.h"
#include "../vulkan/reactive_storage_gpu.h"
#include "../vulkan/runtime_body_gpu.h"
#include "../vulkan/thermal_gpu_state.h"

struct fr_fire_ray_gpu {
    dc_gpu_t *gpu;
    dc_gpu_address_buffer_t commands;
    fr_renderer_compute_t compute;
    uint32_t count;
    fr_player_body_binding_t bindings[DC_FIRE_RAY_MAX];
};
static bool fire_ray_error(char *err,uint32_t cap,const char *text) {
    if(err&&cap) snprintf(err,cap,"%s",text);
    return false;
}

bool fr_fire_ray_gpu_create(dc_gpu_t *g,const fr_player_body_binding_t *b,uint32_t n,
    fr_fire_ray_config_t c,fr_fire_ray_gpu_t **o,char *err,uint32_t cap) {
    if(!g||!b||!n||n>DC_FIRE_RAY_MAX||!o||!c.range||c.range>4096||
       c.half_height>DC_FIRE_RAY_HALF_HEIGHT||!isfinite(c.duration)||c.duration<=0||c.duration>5||
       !isfinite(c.temperature_K)||c.temperature_K<293.15f||c.temperature_K>3000)
        return fire_ray_error(err,cap,"Invalid fire ray configuration");
    for(uint32_t i=0;i<n;i++) {
        if(!b[i].identity||!b[i].body_id||!b[i].generation) return fire_ray_error(err,cap,"Invalid ray emitter ownership");
        for(uint32_t j=0;j<i;j++) if(b[i].identity==b[j].identity||b[i].body_id==b[j].body_id)
            return fire_ray_error(err,cap,"Duplicate ray emitter");
    }
    if(!fr_thermal_gpu_enable(g,err,cap)||!fr_renderer_enable(g,err,cap)) return false;
    fr_fire_ray_gpu_t *r=calloc(1,sizeof(*r));if(!r) return fire_ray_error(err,cap,"Cannot allocate ray adapter");
    r->gpu=g;r->count=n;memcpy(r->bindings,b,n*sizeof(*b));
    VkDeviceSize bytes=(DC_FIRE_RAY_HEADER+(VkDeviceSize)n*DC_FIRE_RAY_WORDS)*4u;
    if(!dc_gpu_address_buffer_create(g,bytes,true,&r->commands,err,cap)) {free(r);return false;}
    uint32_t *w=r->commands.mapped;memset(w,0,(size_t)bytes);w[0]=DC_FIRE_RAY_MAGIC;w[1]=n;
    for(uint32_t i=0;i<n;i++) {
        uint32_t *p=w+DC_FIRE_RAY_HEADER+i*DC_FIRE_RAY_WORDS;
        p[0]=b[i].body_id;p[1]=b[i].generation;p[14]=c.range;p[15]=c.half_height;
        memcpy(p+16,&c.duration,4);memcpy(p+17,&c.temperature_K,4);
    }
    VkDescriptorBufferInfo buffers[]={
        {g->reactive->arena.root.buffer,0,g->reactive->arena.root.bytes},
        {g->page_buffer,0,VK_WHOLE_SIZE},{g->thermal->profiles.buffer,0,g->thermal->profiles.bytes},
        {g->chunk_buffer,0,VK_WHOLE_SIZE},dc_gpu_runtime_bodies_binding(g),
        {g->occupancy_buffer,0,VK_WHOLE_SIZE},{r->commands.buffer,0,bytes}};
    if(!fr_renderer_compute_init(g,&r->compute,buffers,7,64,"build/shaders/player_fire_ray.comp.spv",err,cap)) {
        fr_renderer_compute_destroy(g,&r->compute);dc_gpu_address_buffer_destroy(g,&r->commands);free(r);return false;
    }
    fr_renderer_bind_fire_ray(g,&buffers[6]);*o=r;return true;
}
bool fr_fire_ray_gpu_submit(fr_fire_ray_gpu_t *r,const fr_fire_ray_input_t *i,
    uint32_t n,float dt,char *err,uint32_t cap) {
    if(!r||!i||n!=r->count||!isfinite(dt)||dt<=0||dt>.25f) return fire_ray_error(err,cap,"Invalid ray step");
    for(uint32_t p=0;p<n;p++) if(i[p].identity!=r->bindings[p].identity||!i[p].sequence||
        (i[p].facing!=1&&i[p].facing!= -1)) return fire_ray_error(err,cap,"Invalid ray owner, sequence or facing");
    dc_gpu_t *g=r->gpu;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return fire_ray_error(err,cap,"Cannot reach ray input boundary");
    uint32_t *w=r->commands.mapped;
    for(uint32_t p=0;p<n;p++) {
        uint32_t *c=w+DC_FIRE_RAY_HEADER+p*DC_FIRE_RAY_WORDS;
        c[2]=(uint32_t)i[p].sequence;c[3]=(uint32_t)(i[p].sequence>>32);c[4]=i[p].emit;memcpy(c+5,&i[p].facing,4);
    }
    VkDescriptorBufferInfo root={g->reactive->arena.root.buffer,0,g->reactive->arena.root.bytes},body=dc_gpu_runtime_bodies_binding(g);
    VkWriteDescriptorSet update[2]={
        {.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=r->compute.descriptor,.dstBinding=0,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&root},
        {.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=r->compute.descriptor,.dstBinding=4,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&body}};
    vkUpdateDescriptorSets(g->device,2,update,0,NULL);
    if(!fr_renderer_begin(g)) return false;
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    uint32_t push[16]={g->width,g->height,dc_gpu_pixel_pool_word_offset(g)+DC_GPU_PIXEL_POOL_HEADER_WORDS,0};memcpy(push+3,&dt,4);
    if(g->gas) {
        VkDeviceAddress addresses[]={g->gas->state[g->gas->direct_publish?1u:0u].address,g->gas->state[2].address,
            g->gas->gates.address,g->gas->parameters.address};
        memcpy(push+4,addresses,sizeof(addresses));push[12]=g->gas->sparse_owners;
    }
    push[13]=n;push[14]=g->thermal->profile_count;
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,r->compute.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,r->compute.pipeline_layout,0,1,&r->compute.descriptor,0,NULL);
    vkCmdPushConstants(g->command,r->compute.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,1,1,1);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_HOST_READ_BIT);
    bool okay=fr_renderer_submit(g);
    if(okay&&g->gas) fr_gas_gpu_invalidate_cache(g);
    return okay;
}
bool fr_fire_ray_gpu_stats(fr_fire_ray_gpu_t *r,uint32_t i,fr_fire_ray_stats_t *o) {
    if(!r||!o||i>=r->count||vkQueueWaitIdle(r->gpu->queue)!=VK_SUCCESS) return false;
    uint32_t *c=(uint32_t *)r->commands.mapped+DC_FIRE_RAY_HEADER+i*DC_FIRE_RAY_WORDS;
    *o=(fr_fire_ray_stats_t){.casts=c[9],.heated=c[10],.blocked_rows=c[11]};memcpy(&o->remaining,c+8,4);return true;
}
void fr_fire_ray_gpu_destroy(fr_fire_ray_gpu_t *r) {
    if(!r) return;
    vkQueueWaitIdle(r->gpu->queue);fr_renderer_bind_fire_ray(r->gpu,NULL);
    fr_renderer_compute_destroy(r->gpu,&r->compute);dc_gpu_address_buffer_destroy(r->gpu,&r->commands);free(r);
}
