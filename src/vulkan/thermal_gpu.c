#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dungeoncraft/thermal_water_layout.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal_gpu.h"
#include "reactive_storage_gpu.h"
#include "thermal_feedback_gpu.h"
#include "thermal_gpu_internal.h"

struct fr_thermal_gpu {
    fr_renderer_compute_t compute,feedback;
    dc_gpu_address_buffer_t profiles,samples,fluxes,probe,water;
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
    fr_renderer_compute_destroy(g,&t->feedback);
    dc_gpu_address_buffer_destroy(g,&t->profiles);dc_gpu_address_buffer_destroy(g,&t->samples);
    dc_gpu_address_buffer_destroy(g,&t->fluxes);dc_gpu_address_buffer_destroy(g,&t->probe);
    dc_gpu_address_buffer_destroy(g,&t->water);free(t);
}
void fr_thermal_gpu_destroy(dc_gpu_t *g) {
    if(g) {if(g->thermal_control_mapped) memset(g->thermal_control_mapped,0,128);destroy(g,g->thermal);g->thermal=NULL;}
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
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)count*32u,true,&t->samples,err,cap)||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)count*16u,true,&t->fluxes,err,cap)||
       !dc_gpu_address_buffer_create(g,(VkDeviceSize)DC_GPU_CHUNK_SLOTS*4096u*4u,true,&t->water,err,cap)||
       !dc_gpu_address_buffer_create(g,16,true,&t->probe,err,cap)) {destroy(g,t);return false;}
    ((float *)t->probe.mapped)[0]=FR_THERMAL_PROBE_CAPACITY*(FR_THERMAL_PROBE_K-FR_THERMAL_AMBIENT_K);
    ((float *)t->probe.mapped)[1]=FR_THERMAL_PROBE_CAPACITY;t->overlay=1;
    for(uint32_t i=0;i<t->profile_count;i++) memcpy((fr_reactive_profile_t *)t->profiles.mapped+i,fr_reactive_profile(i),sizeof(fr_reactive_profile_t));
    memset(t->water.mapped,0,(size_t)t->water.bytes);
    VkDescriptorBufferInfo buffers[8]={{g->reactive->arena.root.buffer,0,g->reactive->arena.root.bytes},
        {g->page_buffer,0,VK_WHOLE_SIZE},{t->profiles.buffer,0,t->profiles.bytes},
        {t->samples.buffer,0,t->samples.bytes},{t->fluxes.buffer,0,t->fluxes.bytes},
        {t->probe.buffer,0,t->probe.bytes},{g->chunk_buffer,0,VK_WHOLE_SIZE},{t->water.buffer,0,t->water.bytes}};
    if(getenv("DC_PIPELINE_TRACE")) fprintf(stderr,"thermal pipeline begin\n");
    if(!fr_renderer_compute_init(g,&t->compute,buffers,8,32,"build/shaders/thermal.comp.spv",err,cap)) {destroy(g,t);return false;}
    if(getenv("DC_PIPELINE_TRACE")) fprintf(stderr,"thermal pipeline end\n");
    if(!fr_thermal_feedback_init(g,&t->feedback,buffers[3],buffers[5],err,cap)) {destroy(g,t);return false;}
    uint32_t *control=g->thermal_control_mapped;
    control[2]=(uint32_t)t->water.address;control[3]=(uint32_t)(t->water.address>>32);
    control[4]=DC_GPU_CHUNK_SLOTS*4096u;control[0]=DC_THERMAL_WATER_MAGIC;
    control[DC_THERMAL_PROFILE_ADDRESS_WORD]=(uint32_t)t->profiles.address;
    control[DC_THERMAL_PROFILE_ADDRESS_WORD+1u]=(uint32_t)(t->profiles.address>>32);
    g->thermal=t;return true;
}
static void phase(dc_gpu_t *g,uint32_t mode,float seconds) {
    struct fr_thermal_gpu *t=g->thermal;uint32_t bits=0;memcpy(&bits,&seconds,sizeof(bits));
    uint32_t push[8]={g->width,g->height,mode,bits,0,0,0,t->profile_count};
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
    phase(g,0,seconds);phase(g,5,seconds);phase(g,6,seconds);
    phase(g,0,seconds);phase(g,1,seconds);phase(g,2,seconds);
}
void fr_thermal_record_overlay(dc_gpu_t *g) {
    if(!g->thermal||g->thermal->overlay==0u) return;
    phase(g,0,0);fr_thermal_feedback_record(g,&g->thermal->feedback,g->thermal->overlay,
        g->thermal->groups_x,g->thermal->groups_y);
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
bool fr_thermal_gpu_inspect(dc_gpu_t *g,uint32_t x,uint32_t y,
    fr_thermal_inspection_t *out,char *err,uint32_t cap) {
    if(!g||!g->thermal||!out||x>=g->width||y>=g->height)
        return error(err,cap,"Invalid thermal inspection point");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS||!fr_renderer_begin(g))
        return error(err,cap,"Cannot begin thermal inspection snapshot");
    phase(g,0,0);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    if(!fr_renderer_submit(g)) return error(err,cap,"Cannot complete thermal inspection snapshot");
    uint32_t index=y*g->width+x,count=g->width*g->height;
    const float *node=(const float *)g->thermal->samples.mapped+index*4u;
    const uint32_t *metadata=(const uint32_t *)g->thermal->samples.mapped+(index+count)*4u;
    fr_thermal_inspection_t value={0};
    if(node[1]>0.f) {
        value.occupied=1;value.material=metadata[2];value.heat_capacity=node[1];value.wetness=node[3];
        value.thermal_energy=node[0]+(metadata[0]==2u?node[1]*FR_THERMAL_AMBIENT_K:0.);
        value.temperature_K=value.thermal_energy/value.heat_capacity;
    }
    *out=value;return true;
}
bool fr_thermal_prepare_paint(dc_gpu_t *g,uint32_t x,uint32_t y,uint32_t radius,
    uint32_t material,char *err,uint32_t cap) {
    if(!g||!g->thermal) return true;
    uint32_t *control=g->thermal_control_mapped;control[DC_THERMAL_BRUSH_MATERIAL_WORD]=0u;
    if(material!=DC_MATERIAL_WOOD&&material!=DC_MATERIAL_STONE) return true;
    uint64_t left=x>radius?x-radius:0u,top=y>radius?y-radius:0u;
    uint64_t right=(uint64_t)x+radius+1u,bottom=(uint64_t)y+radius+1u;
    if(right>g->width) right=g->width;
    if(bottom>g->height) bottom=g->height;
    if(left>=right||top>=bottom) return true;
    uint32_t count=(uint32_t)((right-left)*(bottom-top));
    uint64_t base=g->reactive->high_id+1u;
    if(count>=UINT64_MAX-g->reactive->high_id) return error(err,cap,"Thermal parcel identities exceed their address range");
    if(!fr_reactive_arena_reserve_free(g,&g->reactive->arena,count,err,cap)) return false;
    g->reactive->high_id+=count;
    control[DC_THERMAL_BRUSH_ID_WORD]=(uint32_t)base;control[DC_THERMAL_BRUSH_ID_WORD+1u]=(uint32_t)(base>>32);
    control[DC_THERMAL_BRUSH_X_WORD]=(uint32_t)left;control[DC_THERMAL_BRUSH_Y_WORD]=(uint32_t)top;
    control[DC_THERMAL_BRUSH_WIDTH_WORD]=(uint32_t)(right-left);control[DC_THERMAL_BRUSH_MATERIAL_WORD]=material;
    return true;
}
bool fr_thermal_gpu_seed_water(dc_gpu_t *g,uint32_t slot,uint32_t cell,float energy,char *err,uint32_t cap) {
    if(!g||!g->thermal||slot>=DC_GPU_CHUNK_SLOTS||cell>=4096u||!isfinite(energy))
        return error(err,cap,"Invalid liquid heat fixture");
    dc_chunk_t *chunk=malloc(sizeof(*chunk));
    if(!chunk) return error(err,cap,"Cannot validate liquid heat fixture");
    bool okay=dc_gpu_download_chunk(g,slot,chunk,err,cap);
    uint32_t mass=okay?chunk->cells[cell].fluid_mass:0;free(chunk);
    if(!okay) return false;
    if(mass==0u||energy+((float)mass/65536.f)*4.18f*FR_THERMAL_AMBIENT_K<0.f)
        return error(err,cap,"Liquid heat fixture requires water and nonnegative enthalpy");
    ((float *)g->thermal->water.mapped)[slot*4096u+cell]=energy;return true;
}
bool fr_thermal_gpu_read_water_chunk(dc_gpu_t *g,uint32_t slot,float *energy,uint32_t count,char *err,uint32_t cap) {
    if(!g||!g->thermal||slot>=DC_GPU_CHUNK_SLOTS||!energy||count<4096u)
        return error(err,cap,"Invalid liquid heat snapshot");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot complete liquid heat snapshot");
    memcpy(energy,(float *)g->thermal->water.mapped+slot*4096u,4096u*sizeof(float));return true;
}
void fr_thermal_clear_water_chunk(dc_gpu_t *g,uint32_t slot) {
    if(g&&g->thermal&&slot<DC_GPU_CHUNK_SLOTS)
        memset((float *)g->thermal->water.mapped+slot*4096u,0,4096u*sizeof(float));
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
