#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dungeoncraft/gas_layout.h"
#include "ferrum/fire/gas_gpu.h"
#include "body_address_gpu.h"
#include "gas_feedback_gpu.h"
#include "gas_acoustic_gpu.h"
#include "gas_gpu_internal.h"

typedef struct {float words[FR_GAS_CELL_WORDS];} gas_packet_t;
struct fr_gas_gpu {
    fr_renderer_compute_t initialize,prepare,transport,feedback,gate;
    dc_gpu_address_buffer_t state[3],cache[2],parameters,stats,gates;
    fr_gas_model_t model;
    float density,temperature;
    uint32_t groups_x,groups_y;
    fr_gas_gpu_clock_t clock;
    fr_gas_acoustic_gpu_t acoustic;
    uint32_t policy;
    uint32_t cached_bank;
    float cached_rate;
    bool cache_valid;
};
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static void destroy(dc_gpu_t *g,struct fr_gas_gpu *t) {
    if(!t) return;
    fr_gas_acoustic_destroy(g,&t->acoustic);
    fr_renderer_compute_destroy(g,&t->initialize);fr_renderer_compute_destroy(g,&t->prepare);
    fr_renderer_compute_destroy(g,&t->transport);fr_renderer_compute_destroy(g,&t->feedback);
    fr_renderer_compute_destroy(g,&t->gate);dc_gpu_address_buffer_destroy(g,&t->gates);
    for(uint32_t i=0;i<3;i++) dc_gpu_address_buffer_destroy(g,t->state+i);
    for(uint32_t i=0;i<2;i++) dc_gpu_address_buffer_destroy(g,t->cache+i);
    dc_gpu_address_buffer_destroy(g,&t->parameters);dc_gpu_address_buffer_destroy(g,&t->stats);free(t);
}
void fr_gas_gpu_destroy(dc_gpu_t *g) {if(g) {destroy(g,g->gas);g->gas=NULL;}}
void fr_gas_gpu_invalidate_cache(dc_gpu_t *g) {if(g&&g->gas) {g->gas->cache_valid=false;g->gas->acoustic.matrix_valid=false;}}
static void barrier(dc_gpu_t *g) {
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
}
static void dispatch(dc_gpu_t *g,struct fr_gas_gpu *t,const fr_renderer_compute_t *compute,float dt,uint32_t bank) {
    uint32_t bits=0;memcpy(&bits,&dt,4);uint32_t push[4]={g->width,g->height,bits,bank};
    barrier(g);vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,compute->pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,compute->pipeline_layout,0,1,&compute->descriptor,0,NULL);
    vkCmdPushConstants(g->command,compute->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,t->groups_x,t->groups_y,1);
}
static bool submit(dc_gpu_t *g,char *err,uint32_t cap) {
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_HOST_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    return fr_renderer_submit(g)||error(err,cap,"Cannot complete Vulkan gas transaction");
}
static void clear_stats(struct fr_gas_gpu *t) {
    uint32_t *words=t->stats.mapped;words[FR_GAS_RATE_WORD]=0;words[FR_GAS_ERROR_WORD]=0;words[FR_GAS_ERROR_AT_WORD]=UINT32_MAX;
    words[FR_GAS_RATE_X_WORD]=0;words[FR_GAS_RATE_Y_WORD]=0;
}
static bool shader_ok(struct fr_gas_gpu *t,char *err,uint32_t cap) {
    const uint32_t *words=t->stats.mapped;if(!words[FR_GAS_ERROR_WORD]) return true;
    if(err&&cap) snprintf(err,cap,"Invalid Vulkan gas state at physical cell%u",words[FR_GAS_ERROR_AT_WORD]);
    return false;
}
static float rate(struct fr_gas_gpu *t) {
    float value=0;uint32_t *words=t->stats.mapped;
    if(t->policy==FR_GAS_PRESSURE_IMPLICIT) {
        float x=0,y=0;memcpy(&x,words+FR_GAS_RATE_X_WORD,4);memcpy(&y,words+FR_GAS_RATE_Y_WORD,4);return x+y;
    }
    memcpy(&value,words+FR_GAS_RATE_WORD,4);return value;
}
static bool create(dc_gpu_t *g,struct fr_gas_gpu *t,char *err,uint32_t cap) {
    uint32_t count=g->width*g->height;VkDeviceSize states=(VkDeviceSize)DC_GPU_CHUNK_SLOTS*4096u*sizeof(gas_packet_t);
    VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(g->physical,&properties);
    uint32_t groups=(count-1u)/64u+1u;t->groups_x=groups<properties.limits.maxComputeWorkGroupCount[0]?groups:properties.limits.maxComputeWorkGroupCount[0];
    t->groups_y=(groups-1u)/t->groups_x+1u;
    if(states>properties.limits.maxStorageBufferRange||(VkDeviceSize)count*32u>properties.limits.maxStorageBufferRange||
       t->groups_y>properties.limits.maxComputeWorkGroupCount[1]) return error(err,cap,"Gas field exceeds Vulkan address limits");
    for(uint32_t i=0;i<3;i++) if(!dc_gpu_address_buffer_create(g,states,true,t->state+i,err,cap)) return false;
    for(uint32_t i=0;i<2;i++) if(!dc_gpu_address_buffer_create(g,(VkDeviceSize)count*32u,true,t->cache+i,err,cap)) return false;
    if(!dc_gpu_address_buffer_create(g,(VkDeviceSize)DC_GPU_CHUNK_SLOTS*4096u*4u,true,&t->gates,err,cap)) return false;
    if(!dc_gpu_address_buffer_create(g,FR_GAS_MODEL_WORDS*4u,true,&t->parameters,err,cap)||
       !dc_gpu_address_buffer_create(g,64,true,&t->stats,err,cap)) return false;
    float *parameters=t->parameters.mapped;
    for(uint32_t i=0;i<5;i++) {parameters[i]=(float)t->model.gas_constant[i];parameters[5+i]=(float)t->model.heat_capacity_cv[i];}
    parameters[10]=(float)t->model.kinetic_scale;parameters[11]=t->density;parameters[12]=t->temperature;
    parameters[13]=.21f;parameters[14]=0;parameters[15]=6;
    VkDescriptorBufferInfo buffers[12];
    for(uint32_t i=0;i<3;i++) buffers[i]=(VkDescriptorBufferInfo){t->state[i].buffer,0,t->state[i].bytes};
    for(uint32_t i=0;i<2;i++) buffers[3+i]=(VkDescriptorBufferInfo){t->cache[i].buffer,0,t->cache[i].bytes};
    buffers[5]=(VkDescriptorBufferInfo){t->parameters.buffer,0,t->parameters.bytes};buffers[6]=(VkDescriptorBufferInfo){t->stats.buffer,0,t->stats.bytes};
    buffers[7]=(VkDescriptorBufferInfo){g->page_buffer,0,VK_WHOLE_SIZE};buffers[8]=(VkDescriptorBufferInfo){g->chunk_buffer,0,VK_WHOLE_SIZE};
    buffers[9]=(VkDescriptorBufferInfo){g->occupancy_buffer,0,VK_WHOLE_SIZE};buffers[10]=(VkDescriptorBufferInfo){g->mpm_grid_buffer,0,VK_WHOLE_SIZE};
    buffers[11]=(VkDescriptorBufferInfo){t->gates.buffer,0,t->gates.bytes};
    return fr_renderer_compute_init(g,&t->initialize,buffers,12,16,"build/shaders/gas_init.comp.spv",err,cap)&&
        fr_renderer_compute_init(g,&t->prepare,buffers,12,16,"build/shaders/gas_prepare.comp.spv",err,cap)&&
        fr_renderer_compute_init(g,&t->transport,buffers,12,16,"build/shaders/gas_transport.comp.spv",err,cap)&&
        fr_renderer_compute_init(g,&t->gate,buffers,12,32,"build/shaders/gas_gate.comp.spv",err,cap)&&
        fr_gas_feedback_init(g,&t->feedback,buffers[0],buffers[5],buffers[11],err,cap);
}
bool fr_gas_gpu_enable(dc_gpu_t *g,const fr_gas_model_t *model,float density,float temperature,char *err,uint32_t cap) {
    fr_gas_state_t empty={0};fr_gas_thermo_t check;
    if(!g||!model||(uint64_t)g->width*g->height>UINT32_MAX||!isfinite(density)||density<=0||!isfinite(temperature)||temperature<=0||
       !fr_gas_thermodynamics(model,&empty,1,&check,err,cap)||model->gas_constant[0]<=0||model->gas_constant[1]<=0)
        return error(err,cap,"Invalid Vulkan gas configuration");
    fr_gas_model_t rounded=*model;
    for(uint32_t i=0;i<5;i++) {rounded.gas_constant[i]=(float)model->gas_constant[i];rounded.heat_capacity_cv[i]=(float)model->heat_capacity_cv[i];}
    rounded.kinetic_scale=(float)model->kinetic_scale;
    if(!fr_gas_thermodynamics(&rounded,&empty,1,&check,err,cap)||rounded.gas_constant[0]<=0||rounded.gas_constant[1]<=0) return false;
    if(g->gas) return (memcmp(&g->gas->model,&rounded,sizeof(rounded))==0&&g->gas->density==density&&g->gas->temperature==temperature)||
        error(err,cap,"Cannot reinterpret an existing gas field");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot begin gas setup");
    struct fr_gas_gpu *t=calloc(1,sizeof(*t));if(!t) return error(err,cap,"Cannot allocate gas module");
    t->model=rounded;t->density=density;t->temperature=temperature;
    if(!create(g,t,err,cap)||!fr_renderer_begin(g)) {destroy(g,t);return false;}
    dispatch(g,t,&t->initialize,0,0);
    if(!submit(g,err,cap)) {destroy(g,t);return false;}
    g->gas=t;return true;
}
bool fr_gas_gpu_step(dc_gpu_t *g,float seconds,char *err,uint32_t cap) {
    if(!g||!g->gas||!isfinite(seconds)||seconds<0) return error(err,cap,"Invalid Vulkan gas interval");
    if(seconds==0) return true;
    struct fr_gas_gpu *t=g->gas;clear_stats(t);
    bool cached=t->cache_valid;t->cache_valid=false;
    uint32_t bank=cached?t->cached_bank:0;float speed=t->cached_rate;
    VkBufferCopy copy={.size=t->state[0].bytes};
    if(!cached) {
        if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin gas working copy");
        barrier(g);vkCmdCopyBuffer(g->command,t->state[0].buffer,t->state[1].buffer,1,&copy);
        if(t->policy==FR_GAS_PRESSURE_HLLC) vkCmdCopyBuffer(g->command,t->state[0].buffer,t->state[2].buffer,1,&copy);
        dispatch(g,t,&t->prepare,0,0);
        if(!submit(g,err,cap)||!shader_ok(t,err,cap)) return false;
        speed=rate(t);
    }
    double remaining=seconds;uint64_t accepted=0;
    while(remaining>0) {
        if(!isfinite(speed)||speed<0) return error(err,cap,"Invalid reduced gas CFL metadata");
        double courant=t->policy==FR_GAS_PRESSURE_IMPLICIT?FR_GAS_ADVECTIVE_COURANT:FR_GAS_COURANT;
        double allowed=speed>0?courant/speed:remaining;float dt=(float)fmin(remaining,allowed);
        if(dt>allowed) dt=nextafterf(dt,0.f);
        for(;;) {
            if(!(dt>0)||remaining-(double)dt==remaining) return error(err,cap,"Gas interval is below clock precision");
            clear_stats(t);
            if(t->policy==FR_GAS_PRESSURE_IMPLICIT) {
                if(!fr_gas_acoustic_step(g,&t->acoustic,t->groups_x,t->groups_y,dt,bank,&t->stats,err,cap)) return false;
            } else {
                if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin gas substep");
                dispatch(g,t,&t->transport,dt,bank);
                if(!submit(g,err,cap)) return false;
            }
            if(!((uint32_t *)t->stats.mapped)[FR_GAS_ERROR_WORD]) break;
            dt*=.5f;
        }
        remaining-=dt;accepted++;if(t->policy==FR_GAS_PRESSURE_HLLC) bank^=1u;speed=rate(t);
    }
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot publish gas interval");
    barrier(g);vkCmdCopyBuffer(g->command,t->state[1u+bank].buffer,t->state[0].buffer,1,&copy);
    if(!submit(g,err,cap)) return false;
    t->cached_bank=bank;t->cached_rate=speed;t->cache_valid=true;
    t->clock.physical_steps++;t->clock.substeps+=accepted;t->clock.advanced_seconds+=seconds;return true;
}
static fr_gas_state_t unpack(const gas_packet_t *packet) {
    const float *p=packet->words;fr_gas_state_t s={0};
    for(uint32_t i=0;i<5;i++) s.species[i]=p[i];
    s.energy=(double)p[5]+p[11]+p[14];s.momentum[0]=p[6];s.momentum[1]=p[7];
    for(uint32_t i=0;i<3;i++) s.pigment[i]=p[8+i];
    return s;
}
bool fr_gas_gpu_seed_chunk(dc_gpu_t *g,uint32_t slot,const fr_gas_state_t *states,
    const double *volumes,uint32_t count,char *err,uint32_t cap) {
    if(!g||!g->gas||slot>=DC_GPU_CHUNK_SLOTS||!states||!volumes||count!=4096u) return error(err,cap,"Invalid immutable gas import");
    gas_packet_t *packets=calloc(count,sizeof(*packets));if(!packets) return error(err,cap,"Cannot stage gas import");
    bool okay=true;
    for(uint32_t i=0;okay&&i<count;i++) {
        fr_gas_thermo_t t;okay=volumes[i]>=0&&volumes[i]<=1&&fr_gas_thermodynamics(&g->gas->model,states+i,volumes[i],&t,err,cap);
        if(!okay) break;
        float *p=packets[i].words;
        for(uint32_t k=0;k<5;k++) p[k]=(float)states[i].species[k];
        p[5]=(float)states[i].energy;double residual=states[i].energy-p[5];p[11]=(float)residual;p[14]=(float)(residual-p[11]);
        p[6]=(float)states[i].momentum[0];p[7]=(float)states[i].momentum[1];
        for(uint32_t k=0;k<3;k++) p[8+k]=(float)states[i].pigment[k];
        p[12]=p[13]=(float)volumes[i];fr_gas_state_t rounded=unpack(packets+i);
        okay=fr_gas_thermodynamics(&g->gas->model,&rounded,p[12],&t,err,cap);
    }
    if(okay) okay=vkQueueWaitIdle(g->queue)==VK_SUCCESS;
    if(okay) {memcpy((gas_packet_t *)g->gas->state[0].mapped+(size_t)slot*4096u,packets,count*sizeof(*packets));fr_gas_gpu_invalidate_cache(g);}
    free(packets);return okay;
}
bool fr_gas_gpu_read_chunk(dc_gpu_t *g,uint32_t slot,fr_gas_state_t *states,uint32_t count,char *err,uint32_t cap) {
    if(!g||!g->gas||slot>=DC_GPU_CHUNK_SLOTS||!states||count<4096u) return error(err,cap,"Invalid cold gas snapshot");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot complete gas snapshot");
    const gas_packet_t *packets=(const gas_packet_t *)g->gas->state[0].mapped+(size_t)slot*4096u;
    for(uint32_t i=0;i<4096u;i++) states[i]=unpack(packets+i);
    return true;
}
bool fr_gas_gpu_clock(const dc_gpu_t *g,fr_gas_gpu_clock_t *out) {
    if(!g||!g->gas||!out) return false;
    *out=g->gas->clock;return true;
}
void fr_gas_record_overlay(dc_gpu_t *g) {
    if(g->gas) fr_gas_feedback_record(g,&g->gas->feedback,g->gas->groups_x,g->gas->groups_y);
}
bool fr_gas_gpu_set_gravity(dc_gpu_t *g,float gravity,char *err,uint32_t cap) {
    if(!g||!g->gas||!isfinite(gravity)||gravity<0) return error(err,cap,"Invalid gas gravity");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot configure gas gravity");
    ((float *)g->gas->parameters.mapped)[FR_GAS_GRAVITY_WORD]=gravity;return true;
}
bool fr_gas_gpu_set_pressure_policy(dc_gpu_t *g,uint32_t policy,char *err,uint32_t cap) {
    if(!g||!g->gas||policy>FR_GAS_PRESSURE_IMPLICIT) return error(err,cap,"Invalid gas acoustic policy");
    struct fr_gas_gpu *t=g->gas;
    if(policy==FR_GAS_PRESSURE_IMPLICIT&&!t->acoustic.ready) {
        VkDescriptorBufferInfo buffers[12];
        for(uint32_t i=0;i<3;i++) buffers[i]=(VkDescriptorBufferInfo){t->state[i].buffer,0,t->state[i].bytes};
        for(uint32_t i=0;i<2;i++) buffers[3+i]=(VkDescriptorBufferInfo){t->cache[i].buffer,0,t->cache[i].bytes};
        buffers[5]=(VkDescriptorBufferInfo){t->parameters.buffer,0,t->parameters.bytes};buffers[6]=(VkDescriptorBufferInfo){t->stats.buffer,0,t->stats.bytes};
        buffers[7]=(VkDescriptorBufferInfo){g->page_buffer,0,VK_WHOLE_SIZE};buffers[8]=(VkDescriptorBufferInfo){g->chunk_buffer,0,VK_WHOLE_SIZE};
        buffers[9]=(VkDescriptorBufferInfo){g->occupancy_buffer,0,VK_WHOLE_SIZE};buffers[10]=(VkDescriptorBufferInfo){g->mpm_grid_buffer,0,VK_WHOLE_SIZE};
        buffers[11]=(VkDescriptorBufferInfo){t->gates.buffer,0,t->gates.bytes};
        if(!fr_gas_acoustic_create(g,&t->acoustic,buffers,err,cap)) return false;
    }
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot configure acoustic policy");
    if(t->policy!=policy) fr_gas_gpu_invalidate_cache(g);
    ((float *)t->parameters.mapped)[FR_GAS_POLICY_WORD]=(float)policy;t->policy=policy;return true;
}
bool fr_gas_gpu_gate(dc_gpu_t *g,uint32_t x,uint32_t y,uint32_t width,uint32_t height,uint32_t axis,bool closed,char *err,uint32_t cap) {
    if(!g||!g->gas||x>=g->width||y>=g->height||!width||!height||width>g->width-x||height>g->height-y||axis>1u)
        return error(err,cap,"Invalid gas valve rectangle");
    g->gas->acoustic.matrix_valid=false;
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin gas valve command");
    barrier(g);uint32_t push[8]={g->width,g->height,x,y,width,height,axis,closed};
    fr_renderer_compute_t *pipeline=&g->gas->gate;
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline->pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline->pipeline_layout,0,1,&pipeline->descriptor,0,NULL);
    vkCmdPushConstants(g->command,pipeline->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    uint32_t groups=(width*height+63u)/64u,gx=groups<g->gas->groups_x?groups:g->gas->groups_x;
    vkCmdDispatch(g->command,gx,(groups+gx-1u)/gx,1);return submit(g,err,cap);
}
bool fr_gas_gpu_overlay(dc_gpu_t *g,uint32_t mode) {
    if(!g||!g->gas||mode>1u||vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return false;
    ((float *)g->gas->parameters.mapped)[FR_GAS_VIEW_WORD]=(float)mode;return true;
}
bool fr_gas_gpu_read_work_chunk(dc_gpu_t *g,uint32_t slot,double *work,uint32_t count,char *err,uint32_t cap) {
    if(!g||!g->gas||slot>=DC_GPU_CHUNK_SLOTS||!work||count<4096u) return error(err,cap,"Invalid gas work snapshot");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot complete gas work snapshot");
    const gas_packet_t *packets=(const gas_packet_t *)g->gas->state[0].mapped+(size_t)slot*4096u;
    for(uint32_t i=0;i<4096u;i++) work[i]=packets[i].words[15];
    return true;
}
