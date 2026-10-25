#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal_gpu.h"
#include "reactive_storage_gpu.h"
#include "thermal_gpu_internal.h"

struct fr_thermal_gpu {
    fr_renderer_compute_t compute;
    dc_gpu_address_buffer_t profiles,samples,fluxes,probe;
    uint32_t groups_x,groups_y,profile_count,overlay;
    bool probe_active;
};
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static void destroy(dc_gpu_t *g,struct fr_thermal_gpu *t) {
    if(!t) return;
    fr_renderer_compute_destroy(g,&t->compute);
    dc_gpu_address_buffer_destroy(g,&t->profiles);dc_gpu_address_buffer_destroy(g,&t->samples);
    dc_gpu_address_buffer_destroy(g,&t->fluxes);dc_gpu_address_buffer_destroy(g,&t->probe);free(t);
}
void fr_thermal_gpu_destroy(dc_gpu_t *g) {
    if(g) {destroy(g,g->thermal);g->thermal=NULL;}
}
bool fr_thermal_gpu_enable(dc_gpu_t *g,char *err,uint32_t cap) {
    if(!g||!g->runtime_body_consumers||(uint64_t)g->width*g->height>UINT32_MAX)
        return error(err,cap,"Invalid thermal grid context");
    if(g->thermal) return true;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS||!fr_gpu_reactive_initialize(g,err,cap)) return false;
    struct fr_thermal_gpu *t=calloc(1,sizeof(*t));
    if(!t) return error(err,cap,"Cannot allocate thermal module");
    uint32_t count=g->width*g->height;t->profile_count=fr_reactive_profile_count();
    VkPhysicalDeviceProperties props;vkGetPhysicalDeviceProperties(g->physical,&props);
    uint32_t groups=(count-1u)/64u+1u;
    t->groups_x=groups<props.limits.maxComputeWorkGroupCount[0]?groups:props.limits.maxComputeWorkGroupCount[0];
    t->groups_y=(groups-1u)/t->groups_x+1u;
    if(t->groups_y>props.limits.maxComputeWorkGroupCount[1]||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)t->profile_count*sizeof(fr_reactive_profile_t),true,&t->profiles,err,cap)||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)count*16u,true,&t->samples,err,cap)||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)count*8u,true,&t->fluxes,err,cap)||
       !dc_gpu_address_buffer_create(g,16,true,&t->probe,err,cap)) {destroy(g,t);return false;}
    ((float *)t->probe.mapped)[0]=FR_THERMAL_PROBE_CAPACITY*(FR_THERMAL_PROBE_K-FR_THERMAL_AMBIENT_K);
    ((float *)t->probe.mapped)[1]=FR_THERMAL_PROBE_CAPACITY;t->overlay=1;
    for(uint32_t i=0;i<t->profile_count;i++) memcpy((fr_reactive_profile_t *)t->profiles.mapped+i,fr_reactive_profile(i),sizeof(fr_reactive_profile_t));
    VkDescriptorBufferInfo buffers[7]={{g->reactive->arena.root.buffer,0,g->reactive->arena.root.bytes},
        {g->page_buffer,0,VK_WHOLE_SIZE},{t->profiles.buffer,0,t->profiles.bytes},
        {t->samples.buffer,0,t->samples.bytes},{t->fluxes.buffer,0,t->fluxes.bytes},{g->cells,0,VK_WHOLE_SIZE},
        {t->probe.buffer,0,t->probe.bytes}};
    if(!fr_renderer_compute_init(g,&t->compute,buffers,7,32,"build/shaders/thermal.comp.spv",err,cap)) {destroy(g,t);return false;}
    g->thermal=t;return true;
}
static void phase(dc_gpu_t *g,uint32_t mode,float seconds) {
    struct fr_thermal_gpu *t=g->thermal;uint32_t bits=0;memcpy(&bits,&seconds,sizeof(bits));
    uint32_t push[8]={g->width,g->height,mode,bits,g->view_x,g->view_y,t->overlay,t->profile_count};
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,t->compute.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,t->compute.pipeline_layout,0,1,&t->compute.descriptor,0,NULL);
    vkCmdPushConstants(g->command,t->compute.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,mode==4u?1u:t->groups_x,mode==4u?1u:t->groups_y,1);
}
void fr_thermal_record_step(dc_gpu_t *g,float seconds) {
    if(!g->thermal) return;
    if(g->thermal->probe_active) phase(g,4,seconds);
    phase(g,0,seconds);phase(g,1,seconds);phase(g,2,seconds);
}
void fr_thermal_record_overlay(dc_gpu_t *g) {
    if(!g->thermal||g->thermal->overlay==0u) return;
    phase(g,0,0);phase(g,3,0);
}
bool fr_thermal_gpu_probe(dc_gpu_t *g,uint32_t x,uint32_t y,bool active) {
    if(!g||!g->thermal||x>=g->width||y>=g->height) return false;
    uint32_t *input=g->thermal->probe.mapped;input[2]=y*g->width+x;input[3]=active?1u:0u;
    g->thermal->probe_active=active;return true;
}
bool fr_thermal_gpu_overlay(dc_gpu_t *g,uint32_t mode) {
    if(!g||!g->thermal||mode>2u) return false;
    g->thermal->overlay=mode;return true;
}
bool fr_thermal_gpu_step(dc_gpu_t *g,float seconds,char *err,uint32_t cap) {
    if(!g||!g->thermal||!isfinite(seconds)||seconds<0)
        return error(err,cap,"Invalid standalone thermal interval");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS||!fr_renderer_begin(g)) return error(err,cap,"Cannot begin thermal interval");
    fr_thermal_record_step(g,seconds);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    return fr_renderer_submit(g)||error(err,cap,"Cannot complete thermal interval");
}
