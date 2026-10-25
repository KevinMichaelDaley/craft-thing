#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dungeoncraft/gas_layout.h"
#include "gas_acoustic_gpu.h"
#include "gas_gpu_internal.h"
enum { GAS_PRESSURE_SWEEPS=16, GAS_PRESSURE_GROUP=2, GAS_PRACTICAL_SWEEPS=256 };
static bool enabled(const char *name) {const char *value=getenv(name);return !value||strcmp(value,"0")!=0;}
static uint32_t budget(const char *name,uint32_t fallback) {
    const char *value=getenv(name);if(!value) return fallback;
    char *end=NULL;unsigned long count=strtoul(value,&end,10);
    return end!=value&&!*end&&count>0&&count<=256u?(uint32_t)count:fallback;
}
static bool error(char *err,uint32_t cap,const char *message) {if(err&&cap) snprintf(err,cap,"%s",message);return false;}
void fr_gas_acoustic_destroy(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t) {
    fr_renderer_compute_destroy(g,&t->advect);fr_renderer_compute_destroy(g,&t->initialize);
    fr_renderer_compute_destroy(g,&t->rhs);fr_renderer_compute_destroy(g,&t->sor);
    fr_renderer_compute_destroy(g,&t->correct);
    fr_renderer_compute_destroy(g,&t->classify);fr_renderer_compute_destroy(g,&t->compact);
    fr_renderer_compute_destroy(g,&t->residual);fr_renderer_compute_destroy(g,&t->finish);
    fr_renderer_compute_destroy(g,&t->activity);fr_renderer_compute_destroy(g,&t->activity_compact);
    fr_renderer_compute_destroy(g,&t->final_compact);
    fr_renderer_compute_destroy(g,&t->commit);
    fr_renderer_compute_destroy(g,&t->clear_pressure);fr_renderer_compute_destroy(g,&t->prepare_pressure);
    fr_renderer_compute_destroy(g,&t->activity_candidates);
    fr_renderer_compute_destroy(g,&t->transport_factor);fr_renderer_compute_destroy(g,&t->transport_prefix);fr_renderer_compute_destroy(g,&t->transport_apply);
    fr_renderer_compute_destroy(g,&t->transport_metadata);dc_gpu_address_buffer_destroy(g,&t->transport_work);
    fr_renderer_compute_destroy(g,&t->restrict_pressure);fr_renderer_compute_destroy(g,&t->coarse_sor);fr_renderer_compute_destroy(g,&t->prolong_pressure);
    fr_renderer_compute_destroy(g,&t->stage_transport);
    fr_renderer_compute_destroy(g,&t->practical_factor);fr_renderer_compute_destroy(g,&t->practical_prefix);fr_renderer_compute_destroy(g,&t->practical_apply);
    fr_renderer_compute_destroy(g,&t->practical_axis);
    fr_renderer_compute_destroy(g,&t->practical_explicit);
    fr_renderer_compute_destroy(g,&t->temporal_sor);
    for(uint32_t i=0;i<11;i++) dc_gpu_address_buffer_destroy(g,t->fields+i);
    dc_gpu_address_buffer_destroy(g,&t->indirect);
    memset(t,0,sizeof(*t));
}
bool fr_gas_acoustic_create(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t,const VkDescriptorBufferInfo source[12],char *err,uint32_t cap) {
    if(t->ready) return true;
    VkDescriptorBufferInfo buffers[25];memcpy(buffers,source,12*sizeof(*source));
    t->state_bank[0]=source[1].buffer;t->state_bank[1]=source[2].buffer;t->state_bytes=source[1].range;
    VkDeviceSize bytes=(VkDeviceSize)g->width*g->height*16u;
    VkDeviceSize tiles=(VkDeviceSize)((g->width+7u)/8u)*((g->height+7u)/8u);
    VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(g->physical,&properties);
    VkPhysicalDeviceSubgroupProperties subgroup={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    VkPhysicalDeviceFloatControlsProperties floats={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES};subgroup.pNext=&floats;
    VkPhysicalDeviceProperties2 details={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,.pNext=&subgroup};
    vkGetPhysicalDeviceProperties2(g->physical,&details);
    t->supports_large_transport=(subgroup.supportedStages&VK_SHADER_STAGE_COMPUTE_BIT)!=0u&&
        (subgroup.supportedOperations&VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT)!=0u&&subgroup.subgroupSize>0u&&
        subgroup.subgroupSize<=64u&&64u%subgroup.subgroupSize==0u&&floats.shaderDenormPreserveFloat32;
    if(tiles>properties.limits.maxComputeWorkGroupCount[0]) return error(err,cap,"Gas pressure work exceeds device indirect dispatch range");
    for(uint32_t i=0;i<11;i++) {
        VkDeviceSize field_bytes=i==10?bytes:(i==9?tiles*8u:(i==8?bytes/2u:(i>=6?tiles*(i==7?16u:4u):(i==4?bytes/4u:bytes))));
        if(!dc_gpu_address_buffer_create(g,field_bytes,true,t->fields+i,err,cap)) goto fail;
        buffers[12+i]=(VkDescriptorBufferInfo){t->fields[i].buffer,0,field_bytes};
    }
    if(!dc_gpu_address_buffer_create(g,64,true,&t->indirect,err,cap)) goto fail;
    for(uint32_t i=0;i<4;i++) {((uint32_t *)t->indirect.mapped)[i*4]=0u;((uint32_t *)t->indirect.mapped)[i*4+1]=1u;((uint32_t *)t->indirect.mapped)[i*4+2]=1u;}
    buffers[23]=(VkDescriptorBufferInfo){t->indirect.buffer,0,64};
    if(!dc_gpu_address_buffer_create(g,(VkDeviceSize)g->width*g->height*80u,true,&t->transport_work,err,cap)) goto fail;
    buffers[24]=(VkDescriptorBufferInfo){t->transport_work.buffer,0,t->transport_work.bytes};
    fr_renderer_compute_t *pipelines[]={&t->advect,&t->initialize,&t->rhs,&t->classify,&t->compact,&t->sor,&t->correct,&t->residual,&t->finish,&t->activity,&t->activity_compact,&t->final_compact,&t->commit,&t->clear_pressure,&t->prepare_pressure,&t->activity_candidates,&t->transport_factor,&t->transport_prefix,&t->transport_apply,&t->transport_metadata};
    const char *paths[]={"advect","acoustic_init","pressure_rhs","pressure_classify","pressure_compact","pressure_sor","pressure_correct","pressure_residual","acoustic_finish","activity","activity_compact","final_compact","sparse_commit","pressure_clear","pressure_prepare","activity_candidates","transport_factor","transport_prefix","transport_apply","transport_metadata"};
    const char *wide=getenv("DC_GAS_WIDE_SCAN");
    bool use_wide=wide&&strcmp(wide,"0")!=0&&subgroup.subgroupSize>=4u&&properties.limits.maxComputeWorkGroupInvocations>=256u&&properties.limits.maxComputeWorkGroupSize[0]>=256u;
    if(use_wide) {
        paths[17]="transport_prefix_wide";
        paths[16]="transport_factor_wide";
    }
    for(uint32_t i=0;i<20;i++) {
        if(i>=16u&&!t->supports_large_transport) continue;
        char path[128];snprintf(path,sizeof(path),"build/shaders/gas_%s.comp.spv",paths[i]);
        if(!fr_renderer_compute_init(g,pipelines[i],buffers,25,16,path,err,cap)) goto fail;
    }
    fr_renderer_compute_t *coarse_pipelines[]={&t->restrict_pressure,&t->coarse_sor,&t->prolong_pressure};
    const char *coarse_paths[]={"pressure_restrict","pressure_coarse_sor","pressure_prolong"};
    for(uint32_t i=0;i<3;i++) {
        char path[128];snprintf(path,sizeof(path),"build/shaders/gas_%s.comp.spv",coarse_paths[i]);
        if(!fr_renderer_compute_init(g,coarse_pipelines[i],buffers,25,16,path,err,cap)) goto fail;
    }
    if(t->supports_large_transport&&!fr_renderer_compute_init(g,&t->stage_transport,buffers,25,16,"build/shaders/gas_transport_stage.comp.spv",err,cap)) goto fail;
    if(t->supports_large_transport) {
        fr_renderer_compute_t *practical[]={&t->practical_factor,&t->practical_prefix,&t->practical_apply};
        const char *names[]={use_wide?"transport_factor_compact_wide":"transport_factor_compact",
            use_wide?"transport_prefix_compact_wide":"transport_prefix_compact","transport_apply_compact"};
        for(uint32_t i=0;i<3;i++) {
            char path[128];snprintf(path,sizeof(path),"build/shaders/gas_%s.comp.spv",names[i]);
            if(!fr_renderer_compute_init(g,practical[i],buffers,25,16,path,err,cap)) goto fail;
        }
        const char *axis_path=use_wide?"build/shaders/gas_transport_axis_compact_wide.comp.spv":"build/shaders/gas_transport_axis_compact.comp.spv";
        if(!fr_renderer_compute_init(g,&t->practical_axis,buffers,25,16,axis_path,err,cap)) goto fail;
        if(properties.limits.maxComputeSharedMemorySize>=FR_GAS_EXPLICIT_LINE_CAPACITY*48u+64u*4u) {
            const char *explicit_path=use_wide?"build/shaders/gas_transport_explicit_wide.comp.spv":"build/shaders/gas_transport_explicit.comp.spv";
            if(!fr_renderer_compute_init(g,&t->practical_explicit,buffers,25,16,explicit_path,err,cap)) goto fail;
        }
    }
    if(properties.limits.maxComputeWorkGroupInvocations>=128u&&properties.limits.maxComputeWorkGroupSize[0]>=128u&&
       !fr_renderer_compute_init(g,&t->temporal_sor,buffers,25,16,"build/shaders/gas_pressure_temporal.comp.spv",err,cap)) goto fail;
    t->ready=true;return true;
fail:
    fr_gas_acoustic_destroy(g,t);return false;
}
static void barrier(dc_gpu_t *g) {
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
}
static void dispatch(dc_gpu_t *g,const fr_renderer_compute_t *pipeline,uint32_t gx,uint32_t gy,float dt,uint32_t bank) {
    uint32_t bits=0;memcpy(&bits,&dt,4);uint32_t push[4]={g->width,g->height,bits,bank};barrier(g);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline->pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline->pipeline_layout,0,1,&pipeline->descriptor,0,NULL);
    vkCmdPushConstants(g->command,pipeline->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,gx,gy,1);
}
static void indirect_dispatch(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t,const fr_renderer_compute_t *pipeline,float dt,uint32_t bank,VkDeviceSize offset) {
    uint32_t bits=0;memcpy(&bits,&dt,4);uint32_t push[4]={g->width,g->height,bits,bank};
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline->pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline->pipeline_layout,0,1,&pipeline->descriptor,0,NULL);
    vkCmdPushConstants(g->command,pipeline->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatchIndirect(g->command,t->indirect.buffer,offset);
}
static bool submit(dc_gpu_t *g,char *err,uint32_t cap) {
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_HOST_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    return fr_gas_submit(g)||error(err,cap,"Cannot complete acoustic pressure work");
}
static void pressure_sweeps(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t,float dt,uint32_t bank,uint32_t count) {
    const char *setting=getenv("DC_GAS_TEMPORAL_SOR");
    bool temporal=t->temporal_sor.pipeline&&setting&&strcmp(setting,"0")!=0&&count%4u==0u&&(bank&FR_GAS_COMPARE_ROW_SETUP_BIT)==0u;
    if(temporal) {
        VkBufferCopy copy={.size=(VkDeviceSize)g->width*g->height*4u};barrier(g);
        vkCmdCopyBuffer(g->command,t->fields[4].buffer,t->transport_work.buffer,1,&copy);
        for(uint32_t pair=0;pair<count/2u;pair++)
            indirect_dispatch(g,t,&t->temporal_sor,dt,bank|((pair&1u)!=0u?FR_GAS_TEMPORAL_SOURCE_BIT:0u),0);
    } else for(uint32_t sweep=0;sweep<count;sweep++) {
        indirect_dispatch(g,t,&t->sor,dt,bank,0);indirect_dispatch(g,t,&t->sor,dt,bank|2u,0);
    }
}
bool fr_gas_acoustic_step(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t,uint32_t gx,uint32_t gy,
    float dt,uint32_t bank,dc_gpu_address_buffer_t *stats,char *err,uint32_t cap) {
    bool trace=getenv("DC_GAS_TRACE")!=NULL&&g->timestamp_bits;
    uint32_t tail_query=t->large_transport?14u:4u;
    bool matrix_large=t->large_transport||t->practical;
    if(t->matrix_valid&&t->matrix_large!=matrix_large) t->matrix_valid=false;
    uint32_t sweeps=budget("DC_GAS_PRESSURE_SWEEPS",GAS_PRESSURE_SWEEPS),group=budget("DC_GAS_PRESSURE_GROUP",GAS_PRESSURE_GROUP);
    bool compact_large=t->large_transport&&enabled("DC_GAS_SPARSE_LARGE");
    bool sparse=enabled("DC_GAS_SPARSE_RHS")&&t->matrix_valid&&(!t->large_transport||compact_large);
    bool sparse_activity=enabled("DC_GAS_SPARSE_ACTIVITY")&&t->matrix_valid&&(!t->large_transport||compact_large);
    bool sparse_classify=enabled("DC_GAS_SPARSE_CLASSIFY");
    if(sparse) bank|=256u;
    if(sparse_activity) bank|=512u;
    if(sparse_classify) bank|=1024u;
    if(t->large_transport&&!compact_large) bank|=2048u;
    if(matrix_large) bank|=FR_GAS_LARGE_ENERGY_BIT;
    if(t->practical) bank|=FR_GAS_PRACTICAL_BIT;
    if(getenv("DC_GAS_COMPARE_ROW_SETUP")) bank|=FR_GAS_COMPARE_ROW_SETUP_BIT;
    if(!t->matrix_valid) bank|=4u;
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin acoustic advection");
    if(trace) {vkCmdResetQueryPool(g->command,g->timestamp_pool,0,tail_query+2u);vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,0);}
    if(sparse) indirect_dispatch(g,t,&t->clear_pressure,dt,bank,48);
    barrier(g);
    vkCmdFillBuffer(g->command,t->indirect.buffer,32,4,0);vkCmdFillBuffer(g->command,t->indirect.buffer,48,4,0);
    uint32_t tiles=((g->width+7u)/8u)*((g->height+7u)/8u);
    if(sparse_activity) {
        barrier(g);vkCmdFillBuffer(g->command,t->fields[9].buffer,0,tiles*4u,0);
        vkCmdFillBuffer(g->command,t->indirect.buffer,0,4,0);
        dispatch(g,&t->activity_candidates,(tiles+63u)/64u,1u,dt,bank);
        indirect_dispatch(g,t,&t->activity,dt,bank,0);
    } else if(!t->large_transport||compact_large) dispatch(g,&t->activity,(g->width+7u)/8u,(g->height+7u)/8u,dt,bank);
    dispatch(g,&t->activity_compact,(tiles+63u)/64u,1u,dt,bank);
    if(t->large_transport) {
        const char *fused_setting=getenv("DC_GAS_FUSED_APPLY");bool fused=fused_setting&&strcmp(fused_setting,"0")!=0;
        barrier(g);VkBufferCopy copy={.size=t->state_bytes};
        vkCmdCopyBuffer(g->command,t->state_bank[bank&1u],t->state_bank[(bank&1u)^1u],1,&copy);
        if(trace) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,4);
        for(uint32_t axis=0;axis<2;axis++) {
            uint32_t flags=bank|(axis<<3u),lines=axis==0?g->height:g->width;
            if(getenv("DC_GAS_STAGED_RATES")) {
                flags|=FR_GAS_STAGED_RATES_BIT;
                dispatch(g,&t->stage_transport,gx,gy,dt,flags);
            }
            const char *hybrid_setting=getenv("DC_GAS_HYBRID_LINES");
            if(t->practical&&t->practical_explicit.pipeline&&hybrid_setting&&strcmp(hybrid_setting,"0")!=0&&
               (bank&FR_GAS_COMPARE_ROW_SETUP_BIT)==0u) {
                flags|=FR_GAS_HYBRID_LINES_BIT;dispatch(g,&t->practical_explicit,lines,1u,dt,flags);
            }
            if(t->practical&&getenv("DC_GAS_FUSED_AXIS")) {
                dispatch(g,&t->practical_axis,lines,1u,dt,flags);
                if(trace) for(uint32_t stamp=5u;stamp<=8u;stamp++)
                    vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,stamp+axis*4u);
                continue;
            }
            dispatch(g,t->practical?&t->practical_factor:&t->transport_factor,lines,1u,dt,flags);
            if(trace) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,5u+axis*4u);
            dispatch(g,t->practical?&t->practical_prefix:&t->transport_prefix,lines,1u,dt,flags);
            if(trace) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,6u+axis*4u);
            dispatch(g,t->practical?&t->practical_prefix:&t->transport_prefix,lines,1u,dt,flags|16u|(fused?FR_GAS_FUSED_APPLY_BIT:0u));
            if(trace) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,7u+axis*4u);
            if(!fused) dispatch(g,t->practical?&t->practical_apply:&t->transport_apply,gx,gy,dt,flags);
            if(trace) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,8u+axis*4u);
        }
        indirect_dispatch(g,t,&t->transport_metadata,dt,bank,32);
        if(trace) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,13);
    } else indirect_dispatch(g,t,&t->advect,dt,bank,32);
    if(sparse&&(bank&FR_GAS_COMPARE_ROW_SETUP_BIT)==0u) indirect_dispatch(g,t,&t->rhs,dt,bank,32);
    else dispatch(g,&t->rhs,gx,gy,dt,bank&~256u);
    if(trace) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,1);
    uint32_t stalled=0,batches=0;float best=INFINITY;
    for(;;) {
        uint32_t batch_group=t->practical?1u:(batches==0u?1u:group);
        bool coarse=t->practical&&getenv("DC_GAS_COARSE")!=NULL;
        uint32_t batch_sweeps=t->practical?(coarse?32u:GAS_PRACTICAL_SWEEPS):(batches==0u?budget("DC_GAS_FIRST_SWEEPS",sweeps):sweeps);
        for(uint32_t batch=0;batch<batch_group;batch++) {
        barrier(g);vkCmdFillBuffer(g->command,t->indirect.buffer,0,4,0);
        vkCmdFillBuffer(g->command,t->indirect.buffer,16,4,0);
        if(sparse_classify) {
            barrier(g);vkCmdFillBuffer(g->command,t->fields[6].buffer,0,tiles*4u,0);
            vkCmdFillBuffer(g->command,t->indirect.buffer,48,4,0);
            dispatch(g,&t->final_compact,(tiles+63u)/64u,1u,dt,bank);
            indirect_dispatch(g,t,&t->classify,dt,bank,48);
        } else dispatch(g,&t->classify,(g->width+7u)/8u,(g->height+7u)/8u,dt,bank);
        dispatch(g,&t->compact,(tiles+63u)/64u,1u,dt,bank);
        if(sparse) indirect_dispatch(g,t,&t->prepare_pressure,dt,bank,16);
        if(coarse) {
            pressure_sweeps(g,t,dt,bank,16u);
            dispatch(g,&t->restrict_pressure,tiles,1u,dt,bank);
            for(uint32_t sweep=0;sweep<32u;sweep++) {
                dispatch(g,&t->coarse_sor,(tiles+63u)/64u,1u,dt,bank);
                dispatch(g,&t->coarse_sor,(tiles+63u)/64u,1u,dt,bank|2u);
            }
            indirect_dispatch(g,t,&t->prolong_pressure,dt,bank,0);
        }
        pressure_sweeps(g,t,dt,bank,batch_sweeps);
        indirect_dispatch(g,t,&t->correct,dt,bank,0);
        if(trace&&batch+1u==batch_group) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,2);
        barrier(g);vkCmdFillBuffer(g->command,stats->buffer,FR_GAS_RESIDUAL_WORD*4u,4,0);
        if(!t->practical||trace) indirect_dispatch(g,t,&t->residual,dt,bank,16);
        }
        if(trace) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,3);
        barrier(g);vkCmdFillBuffer(g->command,t->indirect.buffer,48,4,0);
        dispatch(g,&t->final_compact,(tiles+63u)/64u,1u,dt,bank);
        indirect_dispatch(g,t,&t->finish,dt,bank,48);
        if(trace) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,tail_query);
        indirect_dispatch(g,t,&t->commit,dt,bank,48);
        if(trace) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,tail_query+1u);
        if(!submit(g,err,cap)) return false;
        if((bank&FR_GAS_COMPARE_ROW_SETUP_BIT)!=0u) {
            uint32_t *words=stats->mapped;float values[4];memcpy(values,words+FR_GAS_ROW_DIAGONAL_BEFORE_WORD,sizeof(values));
            fprintf(stderr,"Gas row compare flags%u count%u first%u cell0 diagonal%.9g/%.9g norm%.9g/%.9g\n",words[FR_GAS_ROW_DIFF_WORD],words[FR_GAS_ROW_DIFF_COUNT_WORD],words[FR_GAS_ROW_DIFF_AT_WORD],values[0],values[1],values[2],values[3]);
        }
        if(trace&&t->large_transport&&batches==0u) {
            uint64_t phases[10];
            if(vkGetQueryPoolResults(g->device,g->timestamp_pool,4,10,sizeof(phases),phases,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)==VK_SUCCESS) {
                fprintf(stderr,"Gas transport GPU ms:");
                for(uint32_t i=1;i<10;i++) fprintf(stderr," %.3f",(phases[i]-phases[i-1u])*g->timestamp_period/1e6);
                fprintf(stderr," (X factor/forward/back/apply, Y factor/forward/back/apply, metadata)\n");
            }
        }
        uint32_t bits=((uint32_t *)stats->mapped)[FR_GAS_RESIDUAL_WORD];float residual=0;memcpy(&residual,&bits,4);
        batches+=batch_group;if(trace)
            fprintf(stderr,"Acoustic dt%.9g batch%u residual%.9g error%u tiles%u advected%u sweeps%u group%u large%u\n",dt,batches,residual,
                ((uint32_t *)stats->mapped)[FR_GAS_ERROR_WORD],((uint32_t *)t->indirect.mapped)[0],((uint32_t *)t->indirect.mapped)[8],batch_sweeps,batch_group,t->large_transport?1u:0u);
        if(trace) {
            uint64_t times[4],tail[2];
            if(vkGetQueryPoolResults(g->device,g->timestamp_pool,0,4,sizeof(times),times,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)==VK_SUCCESS) {
                fprintf(stderr,"Acoustic GPU init%.3f pressure-group%.3f residual%.3f ms\n",(times[1]-times[0])*g->timestamp_period/1e6,
                    (times[2]-times[1])*g->timestamp_period/1e6,(times[3]-times[2])*g->timestamp_period/1e6);
                if(vkGetQueryPoolResults(g->device,g->timestamp_pool,tail_query,2,sizeof(tail),tail,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)==VK_SUCCESS)
                    fprintf(stderr,"Acoustic GPU tail finish%.3f commit%.3f ms\n",(tail[0]-times[3])*g->timestamp_period/1e6,(tail[1]-tail[0])*g->timestamp_period/1e6);
            }
        }
        if(((uint32_t *)stats->mapped)[FR_GAS_ERROR_WORD]) return true;
        if(t->practical) break;
        if(residual<=1e-7f) break;
        if(residual<best*.99f) {best=residual;stalled=0;} else stalled++;
        if(!isfinite(residual)||stalled>=8) {((uint32_t *)stats->mapped)[FR_GAS_ERROR_WORD]=1;return true;}
        if(!fr_renderer_begin(g)) return error(err,cap,"Cannot continue acoustic pressure solve");
        if(trace) {vkCmdResetQueryPool(g->command,g->timestamp_pool,0,tail_query+2u);vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,0);
            vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,1);}
    }
    t->matrix_valid=true;t->matrix_large=matrix_large;return true;
}
