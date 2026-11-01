#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/player_drive_layout.h"
#include "ferrum/player/drive_gpu.h"
#include "runtime_body_internal.h"

struct fr_player_drive_gpu {
    dc_gpu_t *gpu;
    fr_renderer_compute_t compute;
    dc_gpu_address_buffer_t commands;
    uint32_t count;
    fr_player_body_binding_t bindings[];
};

static bool player_drive_error(char *err,uint32_t cap,const char *message) {
    if (err && cap) snprintf(err,cap,"%s",message);
    return false;
}

void fr_player_drive_gpu_destroy(fr_player_drive_gpu_t *d) {
    if (!d) return;
    vkQueueWaitIdle(d->gpu->queue);
    fr_renderer_compute_destroy(d->gpu,&d->compute);
    dc_gpu_address_buffer_destroy(d->gpu,&d->commands);
    free(d);
}

bool fr_player_drive_gpu_create(dc_gpu_t *g,const fr_player_body_binding_t *bindings,
    uint32_t count,float speed,float jump,fr_player_drive_gpu_t **out,char *err,uint32_t cap) {
    uint64_t allocation=sizeof(fr_player_drive_gpu_t)+(uint64_t)count*sizeof(*bindings);
    if (!g || !g->runtime_bodies || !g->runtime_body_consumers || !bindings || !count || !out ||
        allocation>SIZE_MAX || count>UINT32_MAX/FR_PLAYER_DRIVE_WORDS ||
        !isfinite(speed) || !isfinite(jump) || speed<0 || jump<0 || speed>4 || jump>4)
        return player_drive_error(err,cap,"Invalid player body bindings or fixed-tick motion tuning");
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(g->physical,&properties);
    if ((uint64_t)count*FR_PLAYER_DRIVE_WORDS*4>properties.limits.maxStorageBufferRange ||
        ((uint64_t)count+63u)/64u>properties.limits.maxComputeWorkGroupCount[0])
        return player_drive_error(err,cap,"Player input storage exceeds device limits");
    for (uint32_t i=0;i<count;i++) {
        if (!bindings[i].identity || !bindings[i].body_id || !bindings[i].generation)
            return player_drive_error(err,cap,"Player and generation-bearing body identities must be nonzero");
        for (uint32_t j=0;j<i;j++)
            if (bindings[i].identity==bindings[j].identity || bindings[i].body_id==bindings[j].body_id)
                return player_drive_error(err,cap,"Player body bindings must be unique");
    }
    fr_player_drive_gpu_t *d=calloc(1,sizeof(*d)+(size_t)count*sizeof(*bindings));
    if (!d) return player_drive_error(err,cap,"Cannot allocate player drive owner");
    d->gpu=g;d->count=count;
    memcpy(d->bindings,bindings,(size_t)count*sizeof(*bindings));
    if (!dc_gpu_address_buffer_create(g,(VkDeviceSize)count*FR_PLAYER_DRIVE_WORDS*4,true,&d->commands,err,cap))
        goto fail;
    uint32_t *words=d->commands.mapped;
    memset(words,0,(size_t)count*FR_PLAYER_DRIVE_WORDS*4);
    for (uint32_t i=0;i<count;i++) {
        uint32_t *w=words+i*FR_PLAYER_DRIVE_WORDS;
        w[FR_PLAYER_DRIVE_BODY]=bindings[i].body_id;
        w[FR_PLAYER_DRIVE_GENERATION]=bindings[i].generation;
        w[FR_PLAYER_DRIVE_SPEED]=(uint32_t)lroundf(speed*65536.f);
        w[FR_PLAYER_DRIVE_JUMP_SPEED]=(uint32_t)lroundf(jump*65536.f);
    }
    VkDescriptorBufferInfo buffers[]={dc_gpu_runtime_bodies_binding(g),
        {d->commands.buffer,0,d->commands.bytes}};
    if (!fr_renderer_compute_init(g,&d->compute,buffers,2,4,
        "build/shaders/player_drive.comp.spv",err,cap)) goto fail;
    *out=d;
    return true;
fail:
    fr_player_drive_gpu_destroy(d);
    return false;
}

bool fr_player_drive_gpu_submit(fr_player_drive_gpu_t *d,
    const fr_player_drive_input_t *inputs,uint32_t count,char *err,uint32_t cap) {
    if (!d || !inputs || count!=d->count)
        return player_drive_error(err,cap,"Expected one ordered input per bound player");
    for (uint32_t i=0;i<count;i++)
        if (inputs[i].identity!=d->bindings[i].identity || !inputs[i].sequence ||
            !isfinite(inputs[i].move_x) || fabsf(inputs[i].move_x)>1.f)
            return player_drive_error(err,cap,"Invalid player owner, sequence or normalized movement");
    dc_gpu_t *g=d->gpu;
    if (vkQueueWaitIdle(g->queue)!=VK_SUCCESS)
        return player_drive_error(err,cap,"Cannot reach player input boundary");
    uint32_t *words=d->commands.mapped;
    for (uint32_t i=0;i<count;i++) {
        uint32_t *w=words+i*FR_PLAYER_DRIVE_WORDS;
        w[FR_PLAYER_DRIVE_SEQUENCE]=(uint32_t)inputs[i].sequence;
        w[FR_PLAYER_DRIVE_SEQUENCE+1]=(uint32_t)(inputs[i].sequence>>32);
        int32_t move=(int32_t)lroundf(inputs[i].move_x*65536.f);
        memcpy(w+FR_PLAYER_DRIVE_MOVE,&move,sizeof(move));
        w[FR_PLAYER_DRIVE_JUMP]=inputs[i].jump_pressed;
    }
    VkDescriptorBufferInfo root=dc_gpu_runtime_bodies_binding(g);
    VkWriteDescriptorSet update={.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet=d->compute.descriptor,.dstBinding=0,.descriptorCount=1,
        .descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&root};
    vkUpdateDescriptorSets(g->device,1,&update,0,NULL);
    if (!fr_renderer_begin(g)) return player_drive_error(err,cap,"Cannot record player input");
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_HOST_WRITE_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,d->compute.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,d->compute.pipeline_layout,
        0,1,&d->compute.descriptor,0,NULL);
    vkCmdPushConstants(g->command,d->compute.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,4,&count);
    vkCmdDispatch(g->command,(count+63u)/64u,1,1);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_READ_BIT);
    return fr_renderer_submit(g) || player_drive_error(err,cap,"Cannot submit player input");
}
