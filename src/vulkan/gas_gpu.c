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
#include "runtime_body_gpu.h"

#include "gas_gpu_state.h"
bool fr_gas_gpu_reused_line_rate(dc_gpu_t *g,bool *used) {
    if(!g||!g->gas||!used) return false;
    *used=(((uint32_t *)g->gas->stats.mapped)[15]&4u)!=0u;return true;
}
bool fr_gas_gpu_pressure_tiles(const dc_gpu_t *g,uint32_t *tiles) {
    if(!g||!g->gas||!g->gas->acoustic.ready||!tiles) return false;
    *tiles=((const uint32_t *)g->gas->acoustic.indirect.mapped)[0];return true;
}
bool fr_gas_gpu_source_tile_awake(const dc_gpu_t *g,uint32_t x,uint32_t y,bool *awake) {
    if(!g||!g->gas||!g->gas->acoustic.ready||!awake||x>=g->width||y>=g->height) return false;
    uint32_t tile=(y/8u)*((g->width+7u)/8u)+x/8u;
    *awake=((const uint32_t *)g->gas->acoustic.fields[9].mapped)[tile]!=0u;return true;
}
bool fr_gas_gpu_activity_metadata(const dc_gpu_t *g,uint32_t *mode) {
    if(!g||!g->gas||!g->gas->acoustic.ready||!mode) return false;
    *mode=((const uint32_t *)g->gas->stats.mapped)[FR_GAS_ROW_NORM_AFTER_WORD]&1u;return true;
}
bool fr_gas_gpu_hybrid_line_group(const dc_gpu_t *g,uint32_t *width) {
    if(!g||!g->gas||!g->gas->acoustic.practical_hybrid.pipeline||!width) return false;
    *width=g->gas->acoustic.hybrid_line_group;return true;
}
bool fr_gas_gpu_zero_energy_metadata(const dc_gpu_t *g,bool *selected) {
    if(!g||!g->gas||!g->gas->acoustic.ready||!selected) return false;
    *selected=(((const uint32_t *)g->gas->stats.mapped)[FR_GAS_ROW_NORM_AFTER_WORD]&2u)!=0u;return true;
}
bool fr_gas_gpu_sparse_owner_metadata(const dc_gpu_t *g,bool *selected) {
    if(!g||!g->gas||!selected) return false;
    *selected=(((const uint32_t *)g->gas->stats.mapped)[15]&8u)!=0u;return true;
}
bool fr_gas_gpu_geometry_activity_metadata(const dc_gpu_t *g,bool *selected) {
    if(!g||!g->gas||!selected) return false;
    *selected=(((const uint32_t *)g->gas->stats.mapped)[15]&32u)!=0u;return true;
}
bool fr_gas_gpu_sor_tile_size(const dc_gpu_t *g,uint32_t *size) {
    (void)g;if(!size)return false;*size=0;return true;
}
bool fr_gas_gpu_sor_tile_metadata(const dc_gpu_t *g,bool *selected) {
    if(!g||!g->gas||!selected) return false;
    *selected=(((const uint32_t *)g->gas->stats.mapped)[15]&128u)!=0u;return true;
}
bool fr_gas_gpu_pressure_noise_metadata(const dc_gpu_t *g,bool *selected) {
    if(!g||!g->gas||!selected) return false;
    *selected=(((const uint32_t *)g->gas->stats.mapped)[15]&64u)!=0u;return true;
}
bool fr_gas_gpu_work_memory(const dc_gpu_t *g,uint64_t *mapped_bytes,uint64_t *device_bytes) {
    if(!g||!g->gas||!mapped_bytes||!device_bytes) return false;
    const struct fr_gas_gpu *t=g->gas;uint64_t mapped=0,device=0;
    for(uint32_t i=0;i<14;i++) {
        const dc_gpu_address_buffer_t *buffer=i<2u?t->cache+i:(i==2u?&t->acoustic.transport_work:t->acoustic.fields+i-3u);
        if(buffer->host_visible) mapped+=buffer->bytes;else device+=buffer->bytes;
    }
    if(t->owner_tiles.host_visible) mapped+=t->owner_tiles.bytes;else device+=t->owner_tiles.bytes;
    *mapped_bytes=mapped;*device_bytes=device;return true;
}
bool fr_gas_gpu_rate_metadata(const dc_gpu_t *g,float rates[3],uint32_t *width) {
    if(!g||!g->gas||!rates||!width) return false;
    const uint32_t *words=g->gas->stats.mapped;
    memcpy(rates,words+FR_GAS_RATE_WORD,4);memcpy(rates+1,words+FR_GAS_RATE_X_WORD,4);memcpy(rates+2,words+FR_GAS_RATE_Y_WORD,4);
    *width=words[FR_GAS_ROW_NORM_BEFORE_WORD];return true;
}
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static void destroy(dc_gpu_t *g,struct fr_gas_gpu *t) {
    if(!t) return;
    fr_gas_acoustic_destroy(g,&t->acoustic);
    fr_renderer_compute_destroy(g,&t->initialize);fr_renderer_compute_destroy(g,&t->prepare);
    fr_renderer_compute_destroy(g,&t->geometry_stage);fr_renderer_compute_destroy(g,&t->geometry_transfer);fr_renderer_compute_destroy(g,&t->geometry_commit);
    fr_renderer_compute_destroy(g,&t->owner_gather);
    dc_gpu_address_buffer_destroy(g,&t->owner_tiles);
    fr_renderer_compute_destroy(g,&t->transport);fr_renderer_compute_destroy(g,&t->feedback);
    fr_renderer_compute_destroy(g,&t->gate);dc_gpu_address_buffer_destroy(g,&t->gates);
    fr_renderer_compute_destroy(g,&t->audit);dc_gpu_address_buffer_destroy(g,&t->audit_summary);
    for(uint32_t i=0;i<3;i++) dc_gpu_address_buffer_destroy(g,t->state+i);
    for(uint32_t i=0;i<2;i++) dc_gpu_address_buffer_destroy(g,t->cache+i);
    dc_gpu_address_buffer_destroy(g,&t->parameters);dc_gpu_address_buffer_destroy(g,&t->stats);
    dc_gpu_address_buffer_destroy(g,&t->checkpoint_staging);
    if(t->completion) vkDestroyFence(g->device,t->completion,NULL);
    free(t);
}
void fr_gas_gpu_destroy(dc_gpu_t *g) {if(g) {destroy(g,g->gas);g->gas=NULL;}}
bool fr_gas_submit(dc_gpu_t *g) {
    if(!g->gas) return fr_renderer_submit(g);
    VkFence fence=g->gas->completion;
    if(vkEndCommandBuffer(g->command)!=VK_SUCCESS||vkResetFences(g->device,1,&fence)!=VK_SUCCESS) return false;
    VkSubmitInfo submit={.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,.commandBufferCount=1,.pCommandBuffers=&g->command};
    struct fr_gas_gpu *t=g->gas;uint64_t begin=t->trace?SDL_GetPerformanceCounter():0;
    VkResult queued=vkQueueSubmit(g->queue,1,&submit,fence);uint64_t queued_at=t->trace?SDL_GetPerformanceCounter():0;
    VkResult completed=queued==VK_SUCCESS?vkWaitForFences(g->device,1,&fence,VK_TRUE,UINT64_MAX):queued;
    if(t->trace) {t->trace_submits++;t->trace_queue_ticks+=queued_at-begin;t->trace_wait_ticks+=SDL_GetPerformanceCounter()-queued_at;}
    return queued==VK_SUCCESS&&completed==VK_SUCCESS;
}
static bool tick_can_fuse(const dc_gpu_t *g) {
    const char *tick=getenv("DC_GAS_FUSED_TICK"),*geometry=getenv("DC_GAS_FUSED_GEOMETRY"),*local=getenv("DC_GAS_LOCAL_GEOMETRY");
    return g&&g->gas&&g->gas->cache_valid&&g->gas->direct_publish&&g->gas->policy==FR_GAS_PRESSURE_PRACTICAL&&
        tick&&strcmp(tick,"0")!=0&&geometry&&strcmp(geometry,"0")!=0&&local&&strcmp(local,"0")!=0;
}
bool fr_gas_gpu_try_fused_tick(dc_gpu_t *g,float seconds,bool *handled,char *err,uint32_t cap) {
    if(!handled) return error(err,cap,"Invalid fused gas tick result");
    *handled=false;if(!tick_can_fuse(g)) return true;
    if(!dc_gpu_runtime_bodies_service(g,err,cap)) return false;
    if(!tick_can_fuse(g)) return true;
    *handled=true;g->gas->tick_pending=true;
    bool okay=fr_gas_gpu_step(g,seconds,err,cap);g->gas->tick_pending=false;return okay;
}
void fr_gas_gpu_record_pending_tick(dc_gpu_t *g) {
    if(g&&g->gas&&g->gas->tick_pending) {
        g->gas->tick_pending=false;dc_gpu_record_tick_step(g);
    }
}
void fr_gas_gpu_invalidate_cache(dc_gpu_t *g) {if(g&&g->gas) {g->gas->cache_valid=false;g->gas->acoustic.matrix_valid=false;}}
void fr_gas_gpu_geometry_changed(dc_gpu_t *g,bool dynamic) {
    if(g&&g->gas) {g->gas->geometry_dirty=true;g->gas->geometry_dynamic|=dynamic;}
}
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
    return fr_gas_submit(g)||error(err,cap,"Cannot complete Vulkan gas transaction");
}
static void bind_canonical(dc_gpu_t *g,struct fr_gas_gpu *t) {
    uint32_t at=t->direct_publish?1u:0u;
    VkDescriptorBufferInfo buffer={t->state[at].buffer,0,t->state[at].bytes};
    const VkDescriptorSet sets[]={t->initialize.descriptor,t->feedback.descriptor,t->audit.descriptor,
        t->geometry_stage.descriptor,t->geometry_transfer.descriptor,t->geometry_commit.descriptor};
    VkWriteDescriptorSet writes[6];
    for(uint32_t i=0;i<6;i++) writes[i]=(VkWriteDescriptorSet){.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet=sets[i],.dstBinding=0,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,.pBufferInfo=&buffer};
    vkUpdateDescriptorSets(g->device,6,writes,0,NULL);
}
static void record_geometry(dc_gpu_t *g,struct fr_gas_gpu *t,bool local,bool cached) {
    uint32_t *words=t->stats.mapped;words[FR_GAS_ERROR_WORD]=0u;
    bool force=t->geometry_dirty||t->geometry_routes_dirty;
    words[FR_GAS_ROW_NORM_BEFORE_WORD]=local&&force?1u:0u;
    uint32_t bank=(cached?t->cached_bank:0u)|(t->direct_publish?FR_GAS_GEOMETRY_DIRECT_BIT:0u)|
        (local?FR_GAS_GEOMETRY_LOCAL_BIT:0u)|(local&&cached?FR_GAS_GEOMETRY_CACHE_BIT:0u)|
        (local&&force?FR_GAS_GEOMETRY_FORCE_BIT:0u)|
        (t->geometry_routes_dirty&&!t->geometry_dirty&&!t->geometry_dynamic?FR_GAS_GEOMETRY_ROUTES_ONLY_BIT:0u);
    dispatch(g,t,&t->geometry_stage,0,bank);dispatch(g,t,&t->geometry_transfer,0,bank);dispatch(g,t,&t->geometry_commit,0,bank);
}
void fr_gas_gpu_record_pending_geometry(dc_gpu_t *g) {
    struct fr_gas_gpu *t=g->gas;if(!t||!t->geometry_deferred) return;
    record_geometry(g,t,true,true);barrier(g);
    vkCmdFillBuffer(g->command,t->stats.buffer,FR_GAS_RATE_WORD*4u,4u,0u);
    vkCmdFillBuffer(g->command,t->stats.buffer,FR_GAS_RATE_X_WORD*4u,8u,0u);
    t->geometry_deferred=false;
}
bool fr_gas_gpu_refresh_geometry(dc_gpu_t *g,char *err,uint32_t cap) {
    if(!g||!g->gas) return true;
    struct fr_gas_gpu *t=g->gas;if(!t->geometry_dirty&&!t->geometry_dynamic&&!t->geometry_routes_dirty) return true;
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin gas geometry refresh");
    const char *option=getenv("DC_GAS_LOCAL_GEOMETRY");bool local=t->policy==FR_GAS_PRESSURE_PRACTICAL&&option&&strcmp(option,"0")!=0;
    record_geometry(g,t,local,t->cache_valid);uint32_t *words=t->stats.mapped;
    if(!submit(g,err,cap)) return false;
    if(words[FR_GAS_ERROR_WORD]!=0u) return error(err,cap,"Gas geometry candidate is not numerically valid");
    if(words[FR_GAS_ROW_NORM_BEFORE_WORD]!=0u) {
        if(local&&t->cache_valid) {
            float bound=0.;memcpy(&bound,words+FR_GAS_RATE_WORD,4);t->cached_rate=fmaxf(t->cached_rate,bound);t->acoustic.geometry_changed=true;
        } else fr_gas_gpu_invalidate_cache(g);
        t->large_step_hint=0;
    }
    t->geometry_dirty=false;t->geometry_routes_dirty=false;return true;
}
static bool set_direct_publish(dc_gpu_t *g,struct fr_gas_gpu *t,bool direct,char *err,uint32_t cap) {
    if(t->direct_publish==direct) return true;
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot transfer canonical gas ownership");
    VkBufferCopy copy={.size=t->state[0].bytes};barrier(g);
    vkCmdCopyBuffer(g->command,t->state[t->direct_publish?1u:0u].buffer,t->state[direct?1u:0u].buffer,1,&copy);
    if(!submit(g,err,cap)) return false;
    t->direct_publish=direct;bind_canonical(g,t);fr_gas_gpu_invalidate_cache(g);return true;
}
static void clear_stats(struct fr_gas_gpu *t) {
    uint32_t *words=t->stats.mapped;words[FR_GAS_RATE_WORD]=0;words[FR_GAS_ERROR_WORD]=0;words[FR_GAS_ERROR_AT_WORD]=UINT32_MAX;words[FR_GAS_REJECT_STAGE_WORD]=0;
    words[FR_GAS_NEGATIVE_PIGMENT_WORD]=0;words[FR_GAS_EXCESS_PIGMENT_WORD]=0;
    words[FR_GAS_ROW_DIFF_WORD]=0;words[FR_GAS_ROW_DIFF_COUNT_WORD]=0;words[FR_GAS_ROW_DIFF_AT_WORD]=UINT32_MAX;
    words[FR_GAS_RATE_X_WORD]=0;words[FR_GAS_RATE_Y_WORD]=0;
    for(uint32_t i=12;i<16;i++) words[i]=0;
}
static bool shader_ok(dc_gpu_t *g,struct fr_gas_gpu *t,char *err,uint32_t cap) {
    const uint32_t *words=t->stats.mapped;if(!words[FR_GAS_ERROR_WORD]) return true;
    if(getenv("DC_GAS_FAILURE_DIAGNOSTICS")) {
        uint32_t index=words[FR_GAS_ERROR_AT_WORD];float negative=0.,excess=0.;
        memcpy(&negative,words+FR_GAS_NEGATIVE_PIGMENT_WORD,4);memcpy(&excess,words+FR_GAS_EXCESS_PIGMENT_WORD,4);
        fprintf(stderr,"Gas failure steps%llu seconds%.9g flags%u cell%u negative%.9g excess%.9g\n",
            (unsigned long long)t->clock.physical_steps,t->clock.advanced_seconds,words[FR_GAS_REJECT_STAGE_WORD],index,negative,excess);
        if(index<g->width*g->height) {
            uint32_t x=index%g->width,y=index/g->width,page=((const uint32_t *)g->page_mapped)[(y/64u)*g->page_width+x/64u];
            if(page&&page<=DC_GPU_CHUNK_SLOTS) for(uint32_t bank=0;bank<3u;bank++) {
                const gas_packet_t *packets=NULL;char snapshot_error[128];
                if(!fr_gas_gpu_checkpoint_packets(g,bank,page-1u,&packets,snapshot_error,sizeof(snapshot_error))) continue;
                const gas_packet_t *packet=packets+(y%64u)*64u+x%64u;
                fprintf(stderr,"Gas failure bank%u packet",bank);for(uint32_t k=0;k<16u;k++) fprintf(stderr," %.9g",packet->words[k]);fprintf(stderr,"\n");
            }
        }
    }
    if(err&&cap) snprintf(err,cap,"Invalid Vulkan gas state at physical cell%u",words[FR_GAS_ERROR_AT_WORD]);
    return false;
}
static float rate(struct fr_gas_gpu *t) {
    float value=0;uint32_t *words=t->stats.mapped;
    if(t->policy!=FR_GAS_PRESSURE_HLLC) {
        float x=0,y=0;memcpy(&x,words+FR_GAS_RATE_X_WORD,4);memcpy(&y,words+FR_GAS_RATE_Y_WORD,4);return x+y;
    }
    memcpy(&value,words+FR_GAS_RATE_WORD,4);return value;
}
static bool create(dc_gpu_t *g,struct fr_gas_gpu *t,char *err,uint32_t cap) {
    uint32_t count=g->width*g->height;VkDeviceSize states=(VkDeviceSize)DC_GPU_CHUNK_SLOTS*4096u*sizeof(gas_packet_t);
    VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(g->physical,&properties);
    uint32_t scan=count>DC_GPU_CHUNK_SLOTS*4096u?count:DC_GPU_CHUNK_SLOTS*4096u;
    uint32_t groups=(scan-1u)/64u+1u;t->groups_x=groups<properties.limits.maxComputeWorkGroupCount[0]?groups:properties.limits.maxComputeWorkGroupCount[0];
    t->groups_y=(groups-1u)/t->groups_x+1u;
    if(states>properties.limits.maxStorageBufferRange||(VkDeviceSize)count*32u>properties.limits.maxStorageBufferRange||
       t->groups_y>properties.limits.maxComputeWorkGroupCount[1]) return error(err,cap,"Gas field exceeds Vulkan address limits");
    VkFenceCreateInfo fence={.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if(vkCreateFence(g->device,&fence,NULL,&t->completion)!=VK_SUCCESS) return error(err,cap,"Cannot create gas completion fence");
    const char *device_state=getenv("DC_GAS_DEVICE_STATE");bool mapped_state=!device_state||strcmp(device_state,"0")==0;
    for(uint32_t i=0;i<3;i++) if(!dc_gpu_address_buffer_create(g,states,mapped_state,t->state+i,err,cap)) return false;
    if(!mapped_state&&!dc_gpu_address_buffer_create(g,4096u*sizeof(gas_packet_t),true,&t->checkpoint_staging,err,cap)) return false;
    const char *device_work=getenv("DC_GAS_DEVICE_WORK");
    bool mapped_work=!device_work||strcmp(device_work,"0")==0;
    for(uint32_t i=0;i<2;i++) if(!dc_gpu_address_buffer_create(g,(VkDeviceSize)count*32u,mapped_work,t->cache+i,err,cap)) return false;
    VkDeviceSize tiles=(VkDeviceSize)((g->width+7u)/8u)*((g->height+7u)/8u);
    if(!dc_gpu_address_buffer_create(g,tiles*8u,mapped_work,&t->owner_tiles,err,cap)) return false;
    if(!dc_gpu_address_buffer_create(g,(VkDeviceSize)DC_GPU_CHUNK_SLOTS*4096u*4u,true,&t->gates,err,cap)) return false;
    if(!dc_gpu_address_buffer_create(g,(FR_GAS_MODEL_WORDS+DC_GPU_CHUNK_SLOTS)*4u,true,&t->parameters,err,cap)||
       !dc_gpu_address_buffer_create(g,64,true,&t->stats,err,cap)||
       !dc_gpu_address_buffer_create(g,FR_GAS_AUDIT_WORDS*4u,true,&t->audit_summary,err,cap)) return false;
    float *parameters=t->parameters.mapped;
    for(uint32_t i=0;i<5;i++) {parameters[i]=(float)t->model.gas_constant[i];parameters[5+i]=(float)t->model.heat_capacity_cv[i];}
    parameters[10]=(float)t->model.kinetic_scale;parameters[11]=t->density;parameters[12]=t->temperature;
    parameters[13]=.21f;parameters[14]=0;parameters[15]=6;
    ((uint32_t *)t->parameters.mapped)[FR_GAS_SLOT_COUNT_WORD]=DC_GPU_CHUNK_SLOTS;
    ((uint32_t *)t->parameters.mapped)[FR_GAS_CELL_OWNER_WORD]=0u;
    VkDescriptorBufferInfo buffers[27];
    for(uint32_t i=12u;i<27u;i++) buffers[i]=(VkDescriptorBufferInfo){t->parameters.buffer,0,t->parameters.bytes};
    buffers[26]=(VkDescriptorBufferInfo){t->owner_tiles.buffer,0,t->owner_tiles.bytes};
    for(uint32_t i=0;i<3;i++) buffers[i]=(VkDescriptorBufferInfo){t->state[i].buffer,0,t->state[i].bytes};
    for(uint32_t i=0;i<2;i++) buffers[3+i]=(VkDescriptorBufferInfo){t->cache[i].buffer,0,t->cache[i].bytes};
    buffers[5]=(VkDescriptorBufferInfo){t->parameters.buffer,0,t->parameters.bytes};buffers[6]=(VkDescriptorBufferInfo){t->stats.buffer,0,t->stats.bytes};
    buffers[7]=(VkDescriptorBufferInfo){g->page_buffer,0,VK_WHOLE_SIZE};buffers[8]=(VkDescriptorBufferInfo){g->chunk_buffer,0,VK_WHOLE_SIZE};
    buffers[9]=(VkDescriptorBufferInfo){g->occupancy_buffer,0,VK_WHOLE_SIZE};buffers[10]=(VkDescriptorBufferInfo){g->mpm_grid_buffer,0,VK_WHOLE_SIZE};
    buffers[11]=(VkDescriptorBufferInfo){t->gates.buffer,0,t->gates.bytes};
    bool okay=fr_renderer_compute_init(g,&t->initialize,buffers,27,16,"build/shaders/gas_init.comp.spv",err,cap)&&
        fr_renderer_compute_init(g,&t->prepare,buffers,27,16,"build/shaders/gas_prepare.comp.spv",err,cap)&&
        fr_renderer_compute_init(g,&t->transport,buffers,27,16,"build/shaders/gas_transport.comp.spv",err,cap)&&
        fr_renderer_compute_init(g,&t->gate,buffers,27,32,"build/shaders/gas_gate.comp.spv",err,cap)&&
        fr_gas_feedback_init(g,&t->feedback,buffers[0],buffers[2],buffers[5],buffers[11],err,cap)&&
        fr_renderer_compute_init(g,&t->owner_gather,buffers,27,16,"build/shaders/gas_owner_gather.comp.spv",err,cap);
    if(!okay) return false;
    fr_renderer_compute_t *geometry[]={&t->geometry_stage,&t->geometry_transfer,&t->geometry_commit};
    const char *names[]={"stage","transfer","commit"};
    for(uint32_t i=0;i<3;i++) {
        char path[96];snprintf(path,sizeof(path),"build/shaders/gas_geometry_%s.comp.spv",names[i]);
        if(!fr_renderer_compute_init(g,geometry[i],buffers,27,16,path,err,cap)) return false;
    }
    buffers[6]=(VkDescriptorBufferInfo){t->audit_summary.buffer,0,t->audit_summary.bytes};
    return fr_renderer_compute_init(g,&t->audit,buffers,27,16,"build/shaders/gas_audit.comp.spv",err,cap);
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
    vkCmdFillBuffer(g->command,t->owner_tiles.buffer,0,t->owner_tiles.bytes,0u);
    for(uint32_t i=0;i<3;i++) if(!t->state[i].host_visible) vkCmdFillBuffer(g->command,t->state[i].buffer,0,t->state[i].bytes,0);
    for(uint32_t i=0;i<2;i++) if(!t->cache[i].host_visible)
        vkCmdFillBuffer(g->command,t->cache[i].buffer,0,t->cache[i].bytes,0);
    dispatch(g,t,&t->initialize,0,0);
    if(!submit(g,err,cap)) {destroy(g,t);return false;}
    g->gas=t;return true;
}
bool fr_gas_gpu_step(dc_gpu_t *g,float seconds,char *err,uint32_t cap) {
    if(!g||!g->gas||!isfinite(seconds)||seconds<0) return error(err,cap,"Invalid Vulkan gas interval");
    if(seconds==0) return true;
    struct fr_gas_gpu *t=g->gas;t->trace=getenv("DC_GAS_TRACE")!=NULL;clear_stats(t);
    uint64_t start_submits=t->trace_submits,start_queue=t->trace_queue_ticks,start_wait=t->trace_wait_ticks;
    if(g->fluid_work_possible||g->body_count>0u) g->gas->geometry_dynamic=true;
    const char *fusion=getenv("DC_GAS_FUSED_GEOMETRY"),*local=getenv("DC_GAS_LOCAL_GEOMETRY");
    bool fused=t->cache_valid&&t->direct_publish&&t->policy==FR_GAS_PRESSURE_PRACTICAL&&local&&strcmp(local,"0")!=0&&
        fusion&&strcmp(fusion,"0")!=0&&(t->geometry_dirty||t->geometry_dynamic||t->geometry_routes_dirty);
    t->geometry_deferred=fused;
    if(fused) t->acoustic.geometry_changed=true;
    else if(!fr_gas_gpu_refresh_geometry(g,err,cap)) return false;
    bool cached=t->cache_valid;t->cache_valid=false;
    uint32_t bank=cached?t->cached_bank:0;float speed=t->cached_rate;
    VkBufferCopy copy={.size=t->state[0].bytes};
    if(!cached) {
        t->acoustic.matrix_valid=false;
        if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin gas working copy");
        if(!t->direct_publish) {
            barrier(g);vkCmdCopyBuffer(g->command,t->state[0].buffer,t->state[1].buffer,1,&copy);
            if(t->policy==FR_GAS_PRESSURE_HLLC) vkCmdCopyBuffer(g->command,t->state[0].buffer,t->state[2].buffer,1,&copy);
        }
        dispatch(g,t,&t->prepare,0,0);
        if(!submit(g,err,cap)||!shader_ok(g,t,err,cap)) return false;
        speed=rate(t);
    }
    double remaining=seconds,hint=t->large_step_hint>0?t->large_step_hint:seconds;uint64_t accepted=0,retries=0;
    while(remaining>0) {
        if(!isfinite(speed)||speed<0) return error(err,cap,"Invalid reduced gas CFL metadata");
        double courant=t->policy!=FR_GAS_PRESSURE_HLLC?FR_GAS_ADVECTIVE_COURANT:FR_GAS_COURANT;
        double allowed=t->policy==FR_GAS_PRESSURE_PRACTICAL?remaining:(t->policy==FR_GAS_PRESSURE_LARGE_CFL?hint:(speed>0?courant/speed:remaining));
        float dt=(float)fmin(remaining,allowed);bool rejected=false;
        if(dt>allowed) dt=nextafterf(dt,0.f);
        for(;;) {
            if(!(dt>0)||remaining-(double)dt==remaining) return error(err,cap,"Gas interval is below clock precision");
            clear_stats(t);
            if(t->policy!=FR_GAS_PRESSURE_HLLC) {
                double transport_rate=t->policy==FR_GAS_PRESSURE_PRACTICAL?fmax(speed,2.*FR_GAS_PRACTICAL_MIXING_RATE):speed;
                t->acoustic.large_transport=t->policy>=FR_GAS_PRESSURE_LARGE_CFL&&(double)dt*transport_rate>FR_GAS_ADVECTIVE_COURANT;
                if(!fr_gas_acoustic_step(g,&t->acoustic,t->groups_x,t->groups_y,dt,bank,&t->stats,err,cap)) return false;
            } else {
                if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin gas substep");
                dispatch(g,t,&t->transport,dt,bank);
                if(!submit(g,err,cap)) return false;
            }
            if(!((uint32_t *)t->stats.mapped)[FR_GAS_ERROR_WORD]) break;
            if(t->policy==FR_GAS_PRESSURE_PRACTICAL) {
                if(t->trace) fprintf(stderr,"Practical gas rejected interval%.9g flags%u cell%u\n",seconds,((uint32_t *)t->stats.mapped)[FR_GAS_REJECT_STAGE_WORD],((uint32_t *)t->stats.mapped)[FR_GAS_ERROR_AT_WORD]);
                return shader_ok(g,t,err,cap);
            }
            if(getenv("DC_GAS_TRACE")) fprintf(stderr,"Gas retry dt%.9g speed%.9g large%u stages%u cell%u\n",dt,speed,t->acoustic.large_transport?1u:0u,((uint32_t *)t->stats.mapped)[FR_GAS_REJECT_STAGE_WORD],((uint32_t *)t->stats.mapped)[FR_GAS_ERROR_AT_WORD]);
            if(getenv("DC_GAS_TRACE")) {float negative=0,excess=0;memcpy(&negative,(uint32_t *)t->stats.mapped+FR_GAS_NEGATIVE_PIGMENT_WORD,4);memcpy(&excess,(uint32_t *)t->stats.mapped+FR_GAS_EXCESS_PIGMENT_WORD,4);fprintf(stderr,"Gas pigment reject negative%.9g excess%.9g\n",negative,excess);}
            t->acoustic.matrix_valid=false;dt*=.5f;rejected=true;retries++;
            if(t->policy==FR_GAS_PRESSURE_LARGE_CFL) hint=dt;
        }
        if(t->policy==FR_GAS_PRESSURE_LARGE_CFL&&!rejected&&(double)dt>=hint*.9) hint*=1.25;
        remaining-=dt;accepted++;if(t->policy==FR_GAS_PRESSURE_HLLC) bank^=1u;speed=rate(t);
    }
    if(!t->direct_publish) {
        if(!fr_renderer_begin(g)) return error(err,cap,"Cannot publish gas interval");
        barrier(g);vkCmdCopyBuffer(g->command,t->state[1u+bank].buffer,t->state[0].buffer,1,&copy);
        if(!submit(g,err,cap)) return false;
    }
    t->cached_bank=bank;t->cached_rate=speed;t->cache_valid=true;t->acoustic.geometry_changed=false;
    if(fused) {t->geometry_dirty=false;t->geometry_routes_dirty=false;}
    if(t->policy==FR_GAS_PRESSURE_LARGE_CFL) t->large_step_hint=hint;
    t->clock.physical_steps++;t->clock.substeps+=accepted;t->clock.advanced_seconds+=seconds;
    if(t->trace) {
        const char *hybrid=getenv("DC_GAS_HYBRID_LINES");
        if(t->policy==FR_GAS_PRESSURE_PRACTICAL&&hybrid&&strcmp(hybrid,"0")!=0) {
            const uint32_t *words=t->stats.mapped;
            fprintf(stderr,"Gas transport lines explicit%u stages%u implicit%u inactive%u\n",words[FR_GAS_EXPLICIT_LINE_COUNT_WORD],
                words[FR_GAS_EXPLICIT_STAGE_COUNT_WORD],words[FR_GAS_IMPLICIT_LINE_COUNT_WORD],words[FR_GAS_INACTIVE_LINE_COUNT_WORD]);
        }
        VkDeviceSize bytes=t->parameters.bytes+t->stats.bytes+t->gates.bytes+t->audit_summary.bytes+t->acoustic.indirect.bytes+t->acoustic.transport_work.bytes+t->checkpoint_staging.bytes+t->owner_tiles.bytes;
        for(uint32_t i=0;i<3;i++) bytes+=t->state[i].bytes;
        for(uint32_t i=0;i<2;i++) bytes+=t->cache[i].bytes;
        for(uint32_t i=0;i<11;i++) bytes+=t->acoustic.fields[i].bytes;
        double scale=1000./SDL_GetPerformanceFrequency();
        fprintf(stderr,"Gas interval dt%.9g accepted%llu retries%llu submits%llu queue%.3f wait%.3f storage%llu\n",seconds,
            (unsigned long long)accepted,(unsigned long long)retries,(unsigned long long)(t->trace_submits-start_submits),
            (t->trace_queue_ticks-start_queue)*scale,(t->trace_wait_ticks-start_wait)*scale,(unsigned long long)bytes);
    }
    return true;
}
bool fr_gas_gpu_clock(const dc_gpu_t *g,fr_gas_gpu_clock_t *out) {
    if(!g||!g->gas||!out) return false;
    *out=g->gas->clock;return true;
}
bool fr_gas_gpu_queue_ambient_chunk(dc_gpu_t *g,uint32_t slot,char *err,uint32_t cap) {
    if(!g||!g->gas||slot>=DC_GPU_CHUNK_SLOTS) return error(err,cap,"Invalid new gas chunk admission");
    ((uint32_t *)g->gas->parameters.mapped)[FR_GAS_MODEL_WORDS+slot]=1u;
    g->gas->initialization_pending=true;return true;
}
bool fr_gas_gpu_initialize_queued(dc_gpu_t *g,char *err,uint32_t cap) {
    if(!g||!g->gas) return error(err,cap,"Gas module is not enabled");
    struct fr_gas_gpu *t=g->gas;if(!t->initialization_pending) return true;
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin new gas chunk admission");
    dispatch(g,t,&t->initialize,0,0x80000000u);
    if(!submit(g,err,cap)) return false;
    memset((uint32_t *)t->parameters.mapped+FR_GAS_MODEL_WORDS,0,DC_GPU_CHUNK_SLOTS*sizeof(uint32_t));
    t->initialization_pending=false;
    fr_gas_gpu_invalidate_cache(g);t->large_step_hint=0;return true;
}
bool fr_gas_gpu_audit(dc_gpu_t *g,fr_gas_gpu_audit_t *out,char *err,uint32_t cap) {
    if(!g||!g->gas||!out) return error(err,cap,"Invalid cold gas audit");
    struct fr_gas_gpu *t=g->gas;
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS||!fr_renderer_begin(g)) return error(err,cap,"Cannot begin cold gas audit");
    barrier(g);vkCmdFillBuffer(g->command,t->audit_summary.buffer,0,t->audit_summary.bytes,0);barrier(g);
    vkCmdFillBuffer(g->command,t->audit_summary.buffer,FR_GAS_AUDIT_MIN_TEMPERATURE*4u,4u,0x7f7fffffu);
    dispatch(g,t,&t->audit,0,0);
    if(!submit(g,err,cap)) return false;
    memcpy(out->metrics,t->audit_summary.mapped,sizeof(out->metrics));return true;
}
void fr_gas_record_overlay(dc_gpu_t *g) {
    if(g->gas) fr_gas_feedback_record(g,&g->gas->feedback,g->gas->groups_x,g->gas->groups_y);
}
static bool gravity_resolved(const struct fr_gas_gpu *t,float gravity) {
    double first=t->model.gas_constant[0],second=t->model.gas_constant[1];
    double oxygen=(.21/first)/(.21/first+.79/second),reference=(oxygen*first+(1.-oxygen)*second)*t->temperature;
    return t->model.kinetic_scale*gravity<=reference;
}
bool fr_gas_gpu_set_gravity(dc_gpu_t *g,float gravity,char *err,uint32_t cap) {
    if(!g||!g->gas||!isfinite(gravity)||gravity<0) return error(err,cap,"Invalid gas gravity");
    if(g->gas->policy==FR_GAS_PRESSURE_PRACTICAL&&!gravity_resolved(g->gas,gravity)) return error(err,cap,"Gas gravitational scale height is below one physics cell");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot configure gas gravity");
    ((float *)g->gas->parameters.mapped)[FR_GAS_GRAVITY_WORD]=gravity;g->gas->acoustic.matrix_valid=false;return true;
}
bool fr_gas_gpu_set_pressure_policy(dc_gpu_t *g,uint32_t policy,char *err,uint32_t cap) {
    if(!g||!g->gas||policy>FR_GAS_PRESSURE_PRACTICAL) return error(err,cap,"Invalid gas acoustic policy");
    struct fr_gas_gpu *t=g->gas;
    if(policy==FR_GAS_PRESSURE_PRACTICAL&&!gravity_resolved(t,((float *)t->parameters.mapped)[FR_GAS_GRAVITY_WORD])) return error(err,cap,"Gas gravitational scale height is below one physics cell");
    if(policy!=FR_GAS_PRESSURE_HLLC&&!t->acoustic.ready) {
        VkDescriptorBufferInfo buffers[12];
        for(uint32_t i=0;i<3;i++) buffers[i]=(VkDescriptorBufferInfo){t->state[i].buffer,0,t->state[i].bytes};
        for(uint32_t i=0;i<2;i++) buffers[3+i]=(VkDescriptorBufferInfo){t->cache[i].buffer,0,t->cache[i].bytes};
        buffers[5]=(VkDescriptorBufferInfo){t->parameters.buffer,0,t->parameters.bytes};buffers[6]=(VkDescriptorBufferInfo){t->stats.buffer,0,t->stats.bytes};
        buffers[7]=(VkDescriptorBufferInfo){g->page_buffer,0,VK_WHOLE_SIZE};buffers[8]=(VkDescriptorBufferInfo){g->chunk_buffer,0,VK_WHOLE_SIZE};
        buffers[9]=(VkDescriptorBufferInfo){g->occupancy_buffer,0,VK_WHOLE_SIZE};buffers[10]=(VkDescriptorBufferInfo){g->mpm_grid_buffer,0,VK_WHOLE_SIZE};
        buffers[11]=(VkDescriptorBufferInfo){t->gates.buffer,0,t->gates.bytes};
        VkDescriptorBufferInfo owners={t->owner_tiles.buffer,0,t->owner_tiles.bytes};
        if(!fr_gas_acoustic_create(g,&t->acoustic,buffers,owners,err,cap)) return false;
    }
    if(policy>=FR_GAS_PRESSURE_LARGE_CFL&&!t->acoustic.supports_large_transport) return error(err,cap,"Large-CFL transport requires compute subgroup shuffle and float32 denormal preservation");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot configure acoustic policy");
    const char *direct_setting=getenv("DC_GAS_ROTATE_PUBLISH");
    const char *owner_setting=getenv("DC_GAS_SPARSE_OWNERS"),*first_setting=getenv("DC_GAS_DIRECT_FIRST_AXIS");
    bool owners=policy==FR_GAS_PRESSURE_PRACTICAL&&g->width%8u==0u&&g->height%8u==0u&&direct_setting&&strcmp(direct_setting,"0")!=0&&
        owner_setting&&strcmp(owner_setting,"0")!=0&&first_setting&&strcmp(first_setting,"0")!=0&&getenv("DC_GAS_FUSED_AXIS");
    if(t->sparse_owners&&!owners) {
        if(!fr_gas_gpu_materialize_owners(g,UINT32_MAX,true,err,cap)) return false;
        t->sparse_owners=false;((uint32_t *)t->parameters.mapped)[FR_GAS_CELL_OWNER_WORD]=0u;fr_gas_gpu_invalidate_cache(g);
    }
    if(!set_direct_publish(g,t,policy==FR_GAS_PRESSURE_PRACTICAL&&direct_setting&&strcmp(direct_setting,"0")!=0,err,cap)) return false;
    if(owners&&!t->sparse_owners) {t->sparse_owners=true;((uint32_t *)t->parameters.mapped)[FR_GAS_CELL_OWNER_WORD]=1u;fr_gas_gpu_invalidate_cache(g);}
    t->acoustic.sparse_owners=t->sparse_owners;
    if(t->policy!=policy) fr_gas_gpu_invalidate_cache(g);
    ((float *)t->parameters.mapped)[FR_GAS_POLICY_WORD]=(float)policy;t->policy=policy;t->acoustic.practical=policy==FR_GAS_PRESSURE_PRACTICAL;return true;
}
bool fr_gas_gpu_gate(dc_gpu_t *g,uint32_t x,uint32_t y,uint32_t width,uint32_t height,uint32_t axis,bool closed,char *err,uint32_t cap) {
    if(!g||!g->gas||x>=g->width||y>=g->height||!width||!height||width>g->width-x||height>g->height-y||axis>1u)
        return error(err,cap,"Invalid gas valve rectangle");
    g->gas->acoustic.matrix_valid=false;
    g->gas->geometry_routes_dirty=true;
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
