#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dungeoncraft/thermal_water_layout.h"
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/combustion_gpu.h"
#include "ferrum/fire/thermal_gpu.h"
#include "reactive_storage_gpu.h"
#include "thermal_feedback_gpu.h"
#include "thermal_gpu_internal.h"
#include "gas_gpu_state.h"
#include "runtime_body_internal.h"

struct fr_thermal_gpu {
    fr_renderer_compute_t compute,feedback,combustion;
    dc_gpu_address_buffer_t profiles,samples,fluxes,probe,water;
    dc_gpu_address_buffer_t emission,emission_snapshot;
    uint32_t groups_x,groups_y,profile_count,overlay;
    uint32_t purpose,pending_count;
    float *pending_water;
    bool probe_active;
};
bool fr_combustion_gpu_enable(dc_gpu_t *g,char *err,uint32_t cap) {
    if(!g||!g->thermal||!g->gas) {
        if(err&&cap) snprintf(err,cap,"Combustion requires existing thermal and gas owners");
        return false;
    }
    struct fr_thermal_gpu *t=g->thermal;
    if(t->combustion.pipeline) return true;
    if(!fr_gpu_reactive_particle_storage(g,err,cap)) return false;
    if(!dc_gpu_address_buffer_create(g,(VkDeviceSize)DC_GPU_CHUNK_SLOTS*4096u*16u,false,&t->emission,err,cap)) return false;
    VkDescriptorBufferInfo buffers[10]={{g->reactive->arena.root.buffer,0,g->reactive->arena.root.bytes},
        {g->page_buffer,0,VK_WHOLE_SIZE},{t->profiles.buffer,0,t->profiles.bytes},{g->chunk_buffer,0,VK_WHOLE_SIZE},
        {g->cells,0,VK_WHOLE_SIZE},{t->samples.buffer,0,t->samples.bytes},
        {g->particle_buffer,0,VK_WHOLE_SIZE},{g->particle_count_buffer,0,VK_WHOLE_SIZE},
        {t->emission.buffer,0,t->emission.bytes},{t->fluxes.buffer,0,t->fluxes.bytes}};
    bool okay=fr_renderer_compute_init(g,&t->combustion,buffers,10,64,"build/shaders/combustion.comp.spv",err,cap)&&fr_renderer_begin(g);
    if(okay) {
        vkCmdFillBuffer(g->command,t->emission.buffer,0,t->emission.bytes,0);
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        okay=fr_renderer_submit(g);
    }
    if(!okay) {fr_renderer_compute_destroy(g,&t->combustion);dc_gpu_address_buffer_destroy(g,&t->emission);}
    return okay;
}
bool fr_combustion_gpu_enabled(const dc_gpu_t *g) {
    return g&&g->thermal&&g->thermal->combustion.pipeline!=VK_NULL_HANDLE;
}
bool fr_combustion_gpu_read_emission_chunk(dc_gpu_t *g,uint32_t slot,
    fr_combustion_emission_t *out,uint32_t capacity,char *err,uint32_t cap) {
    if(!g||!g->thermal||!g->thermal->emission.buffer||slot>=DC_GPU_CHUNK_SLOTS||!out||capacity<4096u) return false;
    struct fr_thermal_gpu *t=g->thermal;VkDeviceSize bytes=4096u*sizeof(*out);
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return false;
    if(!t->emission_snapshot.buffer&&!dc_gpu_address_buffer_create(g,bytes,true,&t->emission_snapshot,err,cap)) return false;
    if(!fr_renderer_begin(g)) return false;
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_READ_BIT);
    VkBufferCopy copy={(VkDeviceSize)slot*bytes,0,bytes};
    vkCmdCopyBuffer(g->command,t->emission.buffer,t->emission_snapshot.buffer,1,&copy);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    if(!fr_renderer_submit(g)) return false;
    memcpy(out,t->emission_snapshot.mapped,(size_t)bytes);
    uint32_t generation=dc_gpu_structural_data(g)[DC_STRUCTURAL_HEADER_WORDS+slot*8u+4u];
    for(uint32_t i=0;i<4096;i++) if(out[i].generation!=generation) out[i]=(fr_combustion_emission_t){0};
    return true;
}
bool fr_combustion_clear_emission_chunk(dc_gpu_t *g,uint32_t slot,char *err,uint32_t cap) {
    if(!g||!g->thermal||!g->thermal->emission.buffer) return true;
    if(slot>=DC_GPU_CHUNK_SLOTS||!fr_renderer_begin(g)) {
        if(err&&cap) snprintf(err,cap,"Cannot clear cold emission slot");
        return false;
    }
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkDeviceSize bytes=4096u*sizeof(fr_combustion_emission_t);
    vkCmdFillBuffer(g->command,g->thermal->emission.buffer,slot*bytes,bytes,0);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    return fr_renderer_submit(g);
}
static void record_combustion(dc_gpu_t *g,float seconds,uint32_t mode) {
    if(!g||!g->thermal||!g->thermal->combustion.pipeline||!g->gas||(mode==0u&&seconds<=0)) return;
    struct fr_thermal_gpu *t=g->thermal;struct fr_gas_gpu *gas=g->gas;
    uint32_t push[16]={g->width,g->height,0,gas->sparse_owners?1u:0u};memcpy(push+2,&seconds,4);
    VkDeviceAddress addresses[4]={gas->state[gas->direct_publish?1u:0u].address,
        gas->state[2].address,gas->gates.address,gas->parameters.address};
    memcpy(push+4,addresses,sizeof(addresses));
    push[12]=mode;
    if(g->runtime_bodies&&g->runtime_bodies->root.mapped) {
        VkDeviceAddress data=0;const dc_body_storage_root_t *root=g->runtime_bodies->root.mapped;
        memcpy(&data,&root->data,sizeof(data));data+=(VkDeviceAddress)dc_gpu_structural_word_offset(g)*4u;
        memcpy(push+14,&data,sizeof(data));
    }
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,t->combustion.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,t->combustion.pipeline_layout,0,1,&t->combustion.descriptor,0,NULL);
    vkCmdPushConstants(g->command,t->combustion.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,t->groups_x,t->groups_y,1);
    if(mode==0u) {
        /* Reactions preserve gas momentum and never decrease local gas mass.
         * Practical transport uses only the advective rate, so its previous
         * bound remains conservative. Explicit acoustic policies need a new
         * thermal wave-speed bound. Geometry still wakes/repairs ash openings. */
        if(gas->policy!=FR_GAS_PRESSURE_PRACTICAL) fr_gas_gpu_invalidate_cache(g);
        fr_gas_gpu_geometry_changed(g,true);
    }
}
bool fr_combustion_gpu_step(dc_gpu_t *g,float seconds,char *err,uint32_t cap) {
    if(!g||!g->thermal||!g->thermal->combustion.pipeline||!isfinite(seconds)||seconds<0) return false;
    if(seconds==0) return true;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS||!fr_renderer_begin(g)) return false;
    record_combustion(g,seconds,4);record_combustion(g,seconds,5);
    record_combustion(g,seconds,3);record_combustion(g,seconds,0);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    if(fr_renderer_submit(g)) return true;
    if(err&&cap) snprintf(err,cap,"Cannot complete terrain combustion transaction");
    return false;
}
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static void destroy(dc_gpu_t *g,struct fr_thermal_gpu *t) {
    if(!t) return;
    fr_renderer_compute_destroy(g,&t->compute);
    fr_renderer_compute_destroy(g,&t->feedback);
    fr_renderer_compute_destroy(g,&t->combustion);
    dc_gpu_address_buffer_destroy(g,&t->profiles);dc_gpu_address_buffer_destroy(g,&t->samples);
    dc_gpu_address_buffer_destroy(g,&t->fluxes);dc_gpu_address_buffer_destroy(g,&t->probe);
    dc_gpu_address_buffer_destroy(g,&t->water);dc_gpu_address_buffer_destroy(g,&t->emission);
    dc_gpu_address_buffer_destroy(g,&t->emission_snapshot);free(t->pending_water);free(t);
}
void fr_thermal_gpu_destroy(dc_gpu_t *g) {
    if(g) {if(g->thermal_control_mapped) memset(g->thermal_control_mapped,0,128);destroy(g,g->thermal);g->thermal=NULL;}
}
static bool create(dc_gpu_t *g,struct fr_reactive_gpu *reactive,struct fr_thermal_gpu **out,char *err,uint32_t cap) {
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
    VkDescriptorBufferInfo buffers[8]={{reactive->arena.root.buffer,0,reactive->arena.root.bytes},
        {g->page_buffer,0,VK_WHOLE_SIZE},{t->profiles.buffer,0,t->profiles.bytes},
        {t->samples.buffer,0,t->samples.bytes},{t->fluxes.buffer,0,t->fluxes.bytes},
        {t->probe.buffer,0,t->probe.bytes},{g->chunk_buffer,0,VK_WHOLE_SIZE},{t->water.buffer,0,t->water.bytes}};
    if(getenv("DC_PIPELINE_TRACE")) fprintf(stderr,"thermal pipeline begin\n");
    if(!fr_renderer_compute_init(g,&t->compute,buffers,8,32,"build/shaders/thermal.comp.spv",err,cap)) {destroy(g,t);return false;}
    if(getenv("DC_PIPELINE_TRACE")) fprintf(stderr,"thermal pipeline end\n");
    if(!fr_thermal_feedback_init(g,&t->feedback,buffers[3],buffers[5],err,cap)) {destroy(g,t);return false;}
    *out=t;return true;
}
static void bind_control(dc_gpu_t *g,const struct fr_thermal_gpu *t) {
    uint32_t *control=g->thermal_control_mapped;memset(control,0,128);
    if(!t) return;
    control[2]=(uint32_t)t->water.address;control[3]=(uint32_t)(t->water.address>>32);
    control[4]=DC_GPU_CHUNK_SLOTS*4096u;control[0]=DC_THERMAL_WATER_MAGIC;
    control[DC_THERMAL_PROFILE_ADDRESS_WORD]=(uint32_t)t->profiles.address;
    control[DC_THERMAL_PROFILE_ADDRESS_WORD+1u]=(uint32_t)(t->profiles.address>>32);
}
bool fr_thermal_gpu_enable(dc_gpu_t *g,char *err,uint32_t cap) {
    if(!g||!g->runtime_body_consumers||(uint64_t)g->width*g->height>UINT32_MAX)
        return error(err,cap,"Invalid thermal grid context");
    if(g->thermal) return true;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS||!fr_gpu_reactive_initialize(g,err,cap)) return false;
    struct fr_thermal_gpu *t=NULL;
    if(!create(g,g->reactive,&t,err,cap)) return false;
    g->thermal=t;bind_control(g,t);return true;
}
bool fr_thermal_gpu_mark_bench_world(dc_gpu_t *g) {
    if(!g||!g->thermal) return false;
    g->thermal->purpose=FR_THERMAL_WORLD_HEAT_BENCH;return true;
}
bool fr_thermal_gpu_is_bench_world(const dc_gpu_t *g) {
    return g&&g->thermal&&g->thermal->purpose==FR_THERMAL_WORLD_HEAT_BENCH;
}
bool fr_thermal_gpu_mark_bonfire_world(dc_gpu_t *g) {
    if(!g||!g->thermal) return false;
    struct fr_thermal_gpu *t=g->thermal;
    if(t->purpose==FR_THERMAL_WORLD_BONFIRE) return true;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return false;
    ((float *)t->probe.mapped)[0]=200.f*(1500.f-FR_THERMAL_AMBIENT_K);
    ((float *)t->probe.mapped)[1]=200.f;t->purpose=FR_THERMAL_WORLD_BONFIRE;return true;
}
bool fr_thermal_gpu_is_bonfire_world(const dc_gpu_t *g) {
    return g&&g->thermal&&g->thermal->purpose==FR_THERMAL_WORLD_BONFIRE;
}
static void phase(dc_gpu_t *g,uint32_t mode,float seconds) {
    struct fr_thermal_gpu *t=g->thermal;uint32_t bits=0;memcpy(&bits,&seconds,sizeof(bits));
    uint32_t push[8]={g->width,g->height,mode,bits,0,0,t->purpose==FR_THERMAL_WORLD_BONFIRE?1u:0u,t->profile_count};
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,t->compute.pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,t->compute.pipeline_layout,0,1,&t->compute.descriptor,0,NULL);
    vkCmdPushConstants(g->command,t->compute.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,mode==4u?1u:t->groups_x,mode==4u?1u:t->groups_y,1);
}
void fr_thermal_record_step(dc_gpu_t *g,float seconds) {
    if(!g->thermal||seconds<=0) return;
    if(g->thermal->probe_active) phase(g,4,seconds);
    phase(g,0,seconds);phase(g,5,seconds);phase(g,6,seconds);
    phase(g,0,seconds);phase(g,1,seconds);phase(g,2,seconds);
    record_combustion(g,seconds,4);record_combustion(g,seconds,5);
    record_combustion(g,seconds,3);record_combustion(g,seconds,0);
}
void fr_thermal_record_overlay(dc_gpu_t *g) {
    if(!g->thermal) return;
    if(g->thermal->overlay==0u) {record_combustion(g,0,1);return;}
    phase(g,0,0);record_combustion(g,0,2);fr_thermal_feedback_record(g,&g->thermal->feedback,g->thermal->overlay,
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
    record_combustion(g,0,2);
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
bool fr_thermal_gpu_source_state(dc_gpu_t *g,fr_thermal_probe_t *out,char *err,uint32_t cap) {
    if(!g||!g->thermal||!out) return error(err,cap,"Invalid thermal source snapshot");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot complete thermal source snapshot");
    const float *source=g->thermal->probe.mapped;
    *out=(fr_thermal_probe_t){.excess_energy=source[0],.capacity=source[1]};return true;
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
bool fr_thermal_gpu_enabled(const dc_gpu_t *g) {return g&&g->thermal;}
bool fr_thermal_gpu_import_water_chunk(dc_gpu_t *g,uint32_t slot,
    const dc_chunk_t *chunk,const float *energy,char *err,uint32_t cap) {
    if(!g||!g->thermal||slot>=DC_GPU_CHUNK_SLOTS||!chunk||!energy)
        return error(err,cap,"Liquid import requires its world thermal checkpoint");
    double specific=fr_reactive_profile(DC_MATERIAL_WATER)->heat_capacity*FR_THERMAL_AMBIENT_K/65536.;
    for(uint32_t i=0;i<4096u;i++) {
        uint32_t mass=chunk->cells[i].fluid_mass;
        if(!isfinite(energy[i])||(!mass&&energy[i]!=0.f)||mass*specific+energy[i]<0.)
            return error(err,cap,"Streamed liquid heat does not match its physical packet");
    }
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot complete liquid heat import");
    memcpy((float *)g->thermal->water.mapped+slot*4096u,energy,4096u*sizeof(float));return true;
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
static uint32_t resident_slot(dc_gpu_t *g,dc_chunk_coord_t coord) {
    for(uint32_t slot=0;slot<DC_GPU_CHUNK_SLOTS;slot++) {
        const uint32_t *record=dc_gpu_structural_data(g)+DC_STRUCTURAL_HEADER_WORDS+slot*8u;
        if(record[5]&&memcmp(record,&coord,sizeof(coord))==0) return slot;
    }
    return UINT32_MAX;
}
bool fr_thermal_checkpoint_collect(dc_gpu_t *g,const dc_chunk_t *chunks,uint32_t count,
    fr_thermal_checkpoint_t *out,char *err,uint32_t cap) {
    if(!g||!g->thermal||!out||(count&&!chunks)) return error(err,cap,"Invalid thermal save boundary");
    fr_thermal_checkpoint_t value={.purpose=g->thermal->purpose,.chunk_count=count};
    if(!fr_thermal_gpu_source_state(g,&value.source,err,cap)) return false;
    value.water_excess=count?calloc((size_t)count*4096u,sizeof(float)):NULL;
    if(count&&!value.water_excess) return error(err,cap,"Cannot stage thermal checkpoint liquid");
    for(uint32_t i=0;i<count;i++) {
        uint32_t slot=resident_slot(g,chunks[i].coord);bool found=slot!=UINT32_MAX;
        float *destination=value.water_excess+(size_t)i*4096u;
        if(found) memcpy(destination,(float *)g->thermal->water.mapped+(size_t)slot*4096u,4096u*sizeof(float));
        for(uint32_t pending=0;!found&&pending<g->checkpoint_chunk_count;pending++)
            if(memcmp(&g->checkpoint_chunks[pending].coord,&chunks[i].coord,sizeof(chunks[i].coord))==0) {
                if(g->thermal->pending_water&&pending<g->thermal->pending_count)
                    memcpy(destination,g->thermal->pending_water+(size_t)pending*4096u,4096u*sizeof(float));
                found=true;
            }
        if(!found) {fr_thermal_checkpoint_release(&value);return error(err,cap,"Thermal checkpoint has no logical liquid source");}
    }
    *out=value;return true;
}
bool fr_thermal_checkpoint_prepare(dc_gpu_t *g,struct fr_reactive_gpu *reactive,
    const fr_thermal_checkpoint_t *state,const dc_chunk_t *chunks,
    struct fr_thermal_gpu **out,char *err,uint32_t cap) {
    if(!g||!out) return error(err,cap,"Invalid thermal restore transaction");
    if(!state) {*out=NULL;return true;}
    if(!reactive||(state->chunk_count&&!chunks)) return error(err,cap,"Thermal restore needs prepared parcels and physical chunks");
    struct fr_thermal_gpu *t=NULL;
    if(!create(g,reactive,&t,err,cap)) return false;
    t->purpose=state->purpose;t->pending_count=state->chunk_count;
    t->pending_water=state->chunk_count?malloc((size_t)state->chunk_count*4096u*sizeof(float)):NULL;
    if(state->chunk_count&&!t->pending_water) {destroy(g,t);return error(err,cap,"Cannot stage pending thermal chunks");}
    ((float *)t->probe.mapped)[0]=state->source.excess_energy;((float *)t->probe.mapped)[1]=state->source.capacity;
    if(state->chunk_count) memcpy(t->pending_water,state->water_excess,(size_t)state->chunk_count*4096u*sizeof(float));
    for(uint32_t i=0;i<state->chunk_count;i++) {
        uint32_t slot=resident_slot(g,chunks[i].coord);
        if(slot!=UINT32_MAX) memcpy((float *)t->water.mapped+(size_t)slot*4096u,
            state->water_excess+(size_t)i*4096u,4096u*sizeof(float));
    }
    *out=t;return true;
}
void fr_thermal_checkpoint_publish(dc_gpu_t *g,struct fr_thermal_gpu *prepared) {
    struct fr_thermal_gpu *old=g->thermal;g->thermal=prepared;bind_control(g,prepared);destroy(g,old);
}
void fr_thermal_checkpoint_discard(dc_gpu_t *g,struct fr_thermal_gpu *prepared) {destroy(g,prepared);}
bool fr_thermal_checkpoint_preflight_chunk(dc_gpu_t *g,uint32_t index,char *err,uint32_t cap) {
    if(!g||index>=g->checkpoint_chunk_count) return error(err,cap,"Invalid pending thermal index");
    if(!g->thermal||!g->thermal->pending_count) return true;
    return (g->thermal->pending_water&&index<g->thermal->pending_count)||error(err,cap,"Pending thermal packet is inconsistent");
}
bool fr_thermal_checkpoint_bind_chunk(dc_gpu_t *g,uint32_t slot,uint32_t index,char *err,uint32_t cap) {
    if(!fr_thermal_checkpoint_preflight_chunk(g,index,err,cap)||slot>=DC_GPU_CHUNK_SLOTS) return false;
    if(g->thermal&&g->thermal->pending_count) memcpy((float *)g->thermal->water.mapped+(size_t)slot*4096u,
        g->thermal->pending_water+(size_t)index*4096u,4096u*sizeof(float));
    return true;
}
void fr_thermal_checkpoint_remove_pending(dc_gpu_t *g,uint32_t index,uint32_t last) {
    if(!g->thermal||!g->thermal->pending_count) return;
    struct fr_thermal_gpu *t=g->thermal;
    if(index!=last) memcpy(t->pending_water+(size_t)index*4096u,t->pending_water+(size_t)last*4096u,4096u*sizeof(float));
    t->pending_count=last;
    if(!last) {free(t->pending_water);t->pending_water=NULL;}
}
