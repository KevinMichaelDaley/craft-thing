#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dungeoncraft/gas_layout.h"
#include "gas_acoustic_gpu.h"
#include "gas_gpu_internal.h"
enum { GAS_PRESSURE_SWEEPS=16, GAS_PRESSURE_GROUP=2, GAS_PRACTICAL_SWEEPS=256 };
static const float gas_multigrid_max_interval=.05f;
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
    fr_renderer_compute_destroy(g,&t->activity_cached);
    fr_renderer_compute_destroy(g,&t->transport_factor);fr_renderer_compute_destroy(g,&t->transport_prefix);fr_renderer_compute_destroy(g,&t->transport_apply);
    fr_renderer_compute_destroy(g,&t->transport_metadata);dc_gpu_address_buffer_destroy(g,&t->transport_work);
    fr_renderer_compute_destroy(g,&t->restrict_pressure);fr_renderer_compute_destroy(g,&t->coarse_sor);fr_renderer_compute_destroy(g,&t->prolong_pressure);
    fr_renderer_compute_destroy(g,&t->stage_transport);
    fr_renderer_compute_destroy(g,&t->practical_factor);fr_renderer_compute_destroy(g,&t->practical_prefix);fr_renderer_compute_destroy(g,&t->practical_apply);
    fr_renderer_compute_destroy(g,&t->practical_axis);
    fr_renderer_compute_destroy(g,&t->practical_explicit);
    fr_renderer_compute_destroy(g,&t->practical_hybrid);
    for(uint32_t i=0;i<2;i++) fr_renderer_compute_destroy(g,&t->practical_hybrid_axis[i]);
    fr_renderer_compute_destroy(g,&t->temporal_sor);
    fr_renderer_compute_destroy(g,&t->fine_jacobi);
    fr_renderer_compute_destroy(g,&t->coarse_group);
    fr_renderer_compute_destroy(g,&t->rhs_fused);
    fr_renderer_compute_destroy(g,&t->metadata_lean);fr_renderer_compute_destroy(g,&t->rhs_lean);
    fr_renderer_compute_destroy(g,&t->pack_pressure);fr_renderer_compute_destroy(g,&t->packed_sor);
    fr_renderer_compute_destroy(g,&t->reset_pressure_mask);
    fr_renderer_compute_destroy(g,&t->pack_half_pressure);fr_renderer_compute_destroy(g,&t->half_sor);
    for(uint32_t i=0;i<2;i++) fr_renderer_compute_destroy(g,&t->finish_group[i]);
    for(uint32_t i=0;i<3;i++) {
        if(i<2u) {fr_renderer_compute_destroy(g,&t->mg_relax[i]);fr_renderer_compute_destroy(g,&t->mg_temporal[i]);}
        fr_renderer_compute_destroy(g,&t->mg_restrict[i]);fr_renderer_compute_destroy(g,&t->mg_prolong[i]);
        fr_renderer_compute_destroy(g,&t->mg_reconcile[i]);
    }
    for(uint32_t i=0;i<11;i++) dc_gpu_address_buffer_destroy(g,t->fields+i);
    dc_gpu_address_buffer_destroy(g,&t->indirect);
    memset(t,0,sizeof(*t));
}
bool fr_gas_acoustic_create(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t,const VkDescriptorBufferInfo source[12],char *err,uint32_t cap) {
    if(t->ready) return true;
    const char *device_option=getenv("DC_GAS_DEVICE_WORK");
    bool device_work=device_option&&strcmp(device_option,"0")!=0;
    VkDescriptorBufferInfo buffers[25];memcpy(buffers,source,12*sizeof(*source));
    t->state_bank[0]=source[1].buffer;t->state_bank[1]=source[2].buffer;t->state_bytes=source[1].range;
    VkDeviceSize bytes=(VkDeviceSize)g->width*g->height*16u;
    VkDeviceSize tiles=(VkDeviceSize)((g->width+7u)/8u)*((g->height+7u)/8u);
    VkPhysicalDeviceProperties properties;vkGetPhysicalDeviceProperties(g->physical,&properties);
    VkPhysicalDeviceSubgroupProperties subgroup={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    VkPhysicalDeviceFloatControlsProperties floats={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES};subgroup.pNext=&floats;
    VkPhysicalDeviceSubgroupSizeControlProperties sizes={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES};floats.pNext=&sizes;
    VkPhysicalDeviceProperties2 details={.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,.pNext=&subgroup};
    vkGetPhysicalDeviceProperties2(g->physical,&details);
    t->supports_large_transport=(subgroup.supportedStages&VK_SHADER_STAGE_COMPUTE_BIT)!=0u&&
        (subgroup.supportedOperations&VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT)!=0u&&subgroup.subgroupSize>0u&&
        subgroup.subgroupSize<=64u&&64u%subgroup.subgroupSize==0u&&floats.shaderDenormPreserveFloat32;
    if(tiles>properties.limits.maxComputeWorkGroupCount[0]) return error(err,cap,"Gas pressure work exceeds device indirect dispatch range");
    for(uint32_t i=0;i<11;i++) {
        VkDeviceSize field_bytes=i==10?bytes:(i==9?tiles*8u:(i==8?bytes/2u:(i>=6?tiles*(i==7?16u:4u):(i==4?bytes/4u:bytes))));
        if(!dc_gpu_address_buffer_create(g,field_bytes,!device_work||i==9u,t->fields+i,err,cap)) goto fail;
        buffers[12+i]=(VkDescriptorBufferInfo){t->fields[i].buffer,0,field_bytes};
    }
    if(!dc_gpu_address_buffer_create(g,64,true,&t->indirect,err,cap)) goto fail;
    for(uint32_t i=0;i<4;i++) {((uint32_t *)t->indirect.mapped)[i*4]=0u;((uint32_t *)t->indirect.mapped)[i*4+1]=1u;((uint32_t *)t->indirect.mapped)[i*4+2]=1u;}
    buffers[23]=(VkDescriptorBufferInfo){t->indirect.buffer,0,64};
    if(!dc_gpu_address_buffer_create(g,(VkDeviceSize)g->width*g->height*80u,!device_work,&t->transport_work,err,cap)) goto fail;
    buffers[24]=(VkDescriptorBufferInfo){t->transport_work.buffer,0,t->transport_work.bytes};
    fr_renderer_compute_t *pipelines[]={&t->advect,&t->initialize,&t->rhs,&t->classify,&t->compact,&t->sor,&t->correct,&t->residual,&t->finish,&t->activity,&t->activity_compact,&t->final_compact,&t->commit,&t->clear_pressure,&t->prepare_pressure,&t->activity_candidates,&t->transport_factor,&t->transport_prefix,&t->transport_apply,&t->transport_metadata};
    const char *paths[]={"advect","acoustic_init","pressure_rhs","pressure_classify","pressure_compact","pressure_sor","pressure_correct","pressure_residual","acoustic_finish","activity","activity_compact","final_compact","sparse_commit","pressure_clear","pressure_prepare","activity_candidates","transport_factor","transport_prefix","transport_apply","transport_metadata"};
    const char *wide=getenv("DC_GAS_WIDE_SCAN");
    bool use_wide=wide&&strcmp(wide,"0")!=0&&subgroup.subgroupSize>=4u&&properties.limits.maxComputeWorkGroupInvocations>=256u&&properties.limits.maxComputeWorkGroupSize[0]>=256u;
    t->hybrid_line_group=use_wide?256u:64u;
    const char *line_group=getenv("DC_GAS_LINE_GROUP");
    uint32_t minimum_subgroup=sizes.minSubgroupSize?sizes.minSubgroupSize:subgroup.subgroupSize;
    bool large_group=line_group&&strcmp(line_group,"512")==0&&minimum_subgroup>=8u&&
        properties.limits.maxComputeWorkGroupInvocations>=512u&&properties.limits.maxComputeWorkGroupSize[0]>=512u;
    if(large_group) t->hybrid_line_group=512u;
    const char *line_suffix=large_group?"_large":(use_wide?"_wide":"");
    if(use_wide) {
        paths[17]="transport_prefix_wide";
        paths[16]="transport_factor_wide";
    }
    for(uint32_t i=0;i<20;i++) {
        if(i>=16u&&!t->supports_large_transport) continue;
        char path[128];snprintf(path,sizeof(path),"build/shaders/gas_%s.comp.spv",paths[i]);
        if(!fr_renderer_compute_init(g,pipelines[i],buffers,25,16,path,err,cap)) goto fail;
    }
    if(!fr_renderer_compute_init(g,&t->activity_cached,buffers,25,16,"build/shaders/gas_activity_cached.comp.spv",err,cap)) goto fail;
    fr_renderer_compute_t *coarse_pipelines[]={&t->restrict_pressure,&t->coarse_sor,&t->prolong_pressure};
    const char *coarse_paths[]={"pressure_restrict","pressure_coarse_sor","pressure_prolong"};
    for(uint32_t i=0;i<3;i++) {
        char path[128];snprintf(path,sizeof(path),"build/shaders/gas_%s.comp.spv",coarse_paths[i]);
        if(!fr_renderer_compute_init(g,coarse_pipelines[i],buffers,25,16,path,err,cap)) goto fail;
    }
    if(t->supports_large_transport&&!fr_renderer_compute_init(g,&t->stage_transport,buffers,25,16,"build/shaders/gas_transport_stage.comp.spv",err,cap)) goto fail;
    if(t->supports_large_transport) {
        fr_renderer_compute_t *practical[]={&t->practical_factor,&t->practical_prefix,&t->practical_apply};
        const char *names[]={large_group?"transport_factor_compact_large":(use_wide?"transport_factor_compact_wide":"transport_factor_compact"),
            large_group?"transport_prefix_compact_large":(use_wide?"transport_prefix_compact_wide":"transport_prefix_compact"),"transport_apply_compact"};
        for(uint32_t i=0;i<3;i++) {
            char path[128];snprintf(path,sizeof(path),"build/shaders/gas_%s.comp.spv",names[i]);
            if(!fr_renderer_compute_init(g,practical[i],buffers,25,16,path,err,cap)) goto fail;
        }
        const char *axis_path=large_group?"build/shaders/gas_transport_axis_compact_large.comp.spv":(use_wide?"build/shaders/gas_transport_axis_compact_wide.comp.spv":"build/shaders/gas_transport_axis_compact.comp.spv");
        if(!fr_renderer_compute_init(g,&t->practical_axis,buffers,25,16,axis_path,err,cap)) goto fail;
        if(properties.limits.maxComputeSharedMemorySize>=FR_GAS_EXPLICIT_LINE_CAPACITY*48u+64u*4u) {
            const char *explicit_path=large_group?"build/shaders/gas_transport_explicit_large.comp.spv":(use_wide?"build/shaders/gas_transport_explicit_wide.comp.spv":"build/shaders/gas_transport_explicit.comp.spv");
            if(!fr_renderer_compute_init(g,&t->practical_explicit,buffers,25,16,explicit_path,err,cap)) goto fail;
            if(properties.limits.maxComputeSharedMemorySize>=FR_GAS_EXPLICIT_LINE_CAPACITY*48u+64u*4u) {
                const char *hybrid_path=large_group?"build/shaders/gas_transport_hybrid_large.comp.spv":(use_wide?"build/shaders/gas_transport_hybrid_wide.comp.spv":"build/shaders/gas_transport_hybrid.comp.spv");
                if(!fr_renderer_compute_init(g,&t->practical_hybrid,buffers,25,16,hybrid_path,err,cap)) goto fail;
                for(uint32_t axis=0;axis<2;axis++) {
                    char path[128];snprintf(path,sizeof(path),"build/shaders/gas_transport_hybrid_%c%s.comp.spv",axis?'y':'x',line_suffix);
                    if(!fr_renderer_compute_init(g,&t->practical_hybrid_axis[axis],buffers,25,16,path,err,cap)) goto fail;
                }
            }
        }
    }
    if(properties.limits.maxComputeWorkGroupInvocations>=128u&&properties.limits.maxComputeWorkGroupSize[0]>=128u&&
       !fr_renderer_compute_init(g,&t->temporal_sor,buffers,25,16,"build/shaders/gas_pressure_temporal.comp.spv",err,cap)) goto fail;
    if(properties.limits.maxComputeWorkGroupInvocations>=256u&&properties.limits.maxComputeWorkGroupSize[0]>=256u&&
       !fr_renderer_compute_init(g,&t->fine_jacobi,buffers,25,16,"build/shaders/gas_pressure_jacobi_temporal.comp.spv",err,cap)) goto fail;
    if(properties.limits.maxComputeSharedMemorySize>=FR_GAS_COARSE_GROUP_CAPACITY*4u&&
       properties.limits.maxComputeWorkGroupInvocations>=128u&&properties.limits.maxComputeWorkGroupSize[0]>=128u) {
        const char *path=use_wide?"build/shaders/gas_pressure_coarse_group_wide.comp.spv":"build/shaders/gas_pressure_coarse_group.comp.spv";
        if(!fr_renderer_compute_init(g,&t->coarse_group,buffers,25,16,path,err,cap)) goto fail;
    }
    fr_renderer_compute_t *multigrid[]={&t->mg_relax[0],&t->mg_relax[1],&t->mg_restrict[0],&t->mg_restrict[1],&t->mg_restrict[2],&t->mg_prolong[0],&t->mg_prolong[1],&t->mg_prolong[2]};
    const char *mg_names[]={"mg_relax1","mg_relax2","mg_restrict1","mg_restrict2","mg_restrict3","mg_prolong0","mg_prolong1","mg_prolong2"};
    for(uint32_t i=0;i<8;i++) {
        char path[128];snprintf(path,sizeof(path),"build/shaders/gas_%s.comp.spv",mg_names[i]);
        if(!fr_renderer_compute_init(g,multigrid[i],buffers,25,16,path,err,cap)) goto fail;
    }
    for(uint32_t i=0;i<2;i++) {
        char path[128];snprintf(path,sizeof(path),"build/shaders/gas_mg_temporal%u.comp.spv",i+1u);
        if(!fr_renderer_compute_init(g,&t->mg_temporal[i],buffers,25,16,path,err,cap)) goto fail;
    }
    if(!fr_renderer_compute_init(g,&t->rhs_fused,buffers,25,16,"build/shaders/gas_pressure_rhs_fused.comp.spv",err,cap)) goto fail;
    if(!fr_renderer_compute_init(g,&t->metadata_lean,buffers,25,16,"build/shaders/gas_transport_metadata_lean.comp.spv",err,cap)||
       !fr_renderer_compute_init(g,&t->rhs_lean,buffers,25,16,"build/shaders/gas_pressure_rhs_lean.comp.spv",err,cap)) goto fail;
    if(t->temporal_sor.pipeline) {
        if(!fr_renderer_compute_init(g,&t->pack_pressure,buffers,25,16,"build/shaders/gas_pressure_pack.comp.spv",err,cap)||
           !fr_renderer_compute_init(g,&t->packed_sor,buffers,25,16,"build/shaders/gas_pressure_temporal_packed.comp.spv",err,cap)) goto fail;
        if(!fr_renderer_compute_init(g,&t->pack_half_pressure,buffers,25,16,"build/shaders/gas_pressure_pack_half.comp.spv",err,cap)||
           !fr_renderer_compute_init(g,&t->half_sor,buffers,25,16,"build/shaders/gas_pressure_temporal_half.comp.spv",err,cap)) goto fail;
    }
    if(!fr_renderer_compute_init(g,&t->reset_pressure_mask,buffers,25,16,"build/shaders/gas_pressure_mask_reset.comp.spv",err,cap)) goto fail;
    for(uint32_t i=0;i<3;i++) {
        char path[128];snprintf(path,sizeof(path),"build/shaders/gas_mg_reconcile%u.comp.spv",i+1u);
        if(!fr_renderer_compute_init(g,&t->mg_reconcile[i],buffers,25,16,path,err,cap)) goto fail;
    }
    for(uint32_t i=0;i<2;i++) {
        uint32_t size=128u<<i;if(properties.limits.maxComputeWorkGroupInvocations<size||properties.limits.maxComputeWorkGroupSize[0]<size||properties.limits.maxComputeSharedMemorySize<size*16u) continue;
        char path[128];snprintf(path,sizeof(path),"build/shaders/gas_acoustic_finish_group%u.comp.spv",size);
        if(!fr_renderer_compute_init(g,&t->finish_group[i],buffers,25,16,path,err,cap)) goto fail;
    }
    if(device_work) {
        if(!fr_renderer_begin(g)) goto fail;
        for(uint32_t i=0;i<11;i++) if(!t->fields[i].host_visible)
            vkCmdFillBuffer(g->command,t->fields[i].buffer,0,t->fields[i].bytes,0);
        vkCmdFillBuffer(g->command,t->transport_work.buffer,0,t->transport_work.bytes,0);
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
        if(!fr_gas_submit(g)) goto fail;
    }
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
    uint32_t bits=0;memcpy(&bits,&dt,4);uint32_t push[4]={g->width,g->height,bits,bank};
    const char *setting=getenv("DC_GAS_COMPUTE_BARRIERS");
    if(setting&&strcmp(setting,"0")!=0)
        fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    else barrier(g);
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
            indirect_dispatch(g,t,(bank&FR_GAS_HALF_PRESSURE_BIT)!=0u?&t->half_sor:((bank&FR_GAS_PACKED_PRESSURE_BIT)!=0u?&t->packed_sor:&t->temporal_sor),
                dt,bank|((pair&1u)!=0u?FR_GAS_TEMPORAL_SOURCE_BIT:0u),0);
    } else for(uint32_t sweep=0;sweep<count;sweep++) {
        indirect_dispatch(g,t,&t->sor,dt,bank,0);indirect_dispatch(g,t,&t->sor,dt,bank|2u,0);
    }
}
static void multigrid_dispatch(dc_gpu_t *g,const fr_renderer_compute_t *pipeline,float dt,uint32_t bank,uint32_t level) {
    uint32_t scale=1u<<level,width=(g->width+scale-1u)/scale,height=(g->height+scale-1u)/scale;
    uint32_t count=(width*height+63u)/64u,max_x=(g->width+7u)/8u;
    uint32_t x=count<max_x?count:max_x;
    dispatch(g,pipeline,x,(count+x-1u)/x,dt,bank|(level<<20u));
}
static void multigrid_smooth(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t,float dt,uint32_t bank,uint32_t level,bool post) {
    bool boundary=(bank&FR_GAS_MG_BOUNDARY_BIT)!=0u;
    if(level==0u&&(bank&FR_GAS_MG_JACOBI_BIT)!=0u) {
        VkBufferCopy copy={.size=(VkDeviceSize)g->width*g->height*4u};barrier(g);
        vkCmdCopyBuffer(g->command,t->fields[4].buffer,t->transport_work.buffer,1,&copy);
        for(uint32_t pair=0u;pair<4u;pair++) indirect_dispatch(g,t,&t->fine_jacobi,dt,
            bank|((pair&1u)!=0u?FR_GAS_TEMPORAL_SOURCE_BIT:0u),0);
        return;
    }
    if(level==0u) {pressure_sweeps(g,t,dt,bank,boundary?(post?12u:6u):12u);return;}
    uint32_t coarse_sweeps=boundary&&level==2u&&!post?2u:4u;
    const char *setting=getenv("DC_GAS_MG_TEMPORAL");
    if(coarse_sweeps==4u&&setting&&strcmp(setting,"0")!=0) {
        uint32_t scale=1u<<level,width=(g->width+scale-1u)/scale,height=(g->height+scale-1u)/scale;
        for(uint32_t pair=0;pair<2u;pair++) dispatch(g,&t->mg_temporal[level-1u],(width+7u)/8u,(height+7u)/8u,
            dt,bank|((pair&1u)!=0u?1u<<24u:0u));
        return;
    }
    for(uint32_t i=0;i<coarse_sweeps;i++) multigrid_dispatch(g,&t->mg_relax[level-1u],dt,bank|((i&1u)!=0u?1u<<24u:0u),level);
}
static void multigrid_cycle(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t,float dt,uint32_t bank,uint32_t tiles) {
    for(uint32_t level=0u;level<3u;level++) {
        multigrid_smooth(g,t,dt,bank,level,false);
        multigrid_dispatch(g,&t->mg_restrict[level],dt,bank,level+1u);
        if((bank&FR_GAS_MG_BOUNDARY_BIT)!=0u) multigrid_dispatch(g,&t->mg_reconcile[level],dt,bank,level+1u);
    }
    if(t->coarse_group.pipeline&&tiles<=FR_GAS_COARSE_GROUP_CAPACITY) dispatch(g,&t->coarse_group,1u,1u,dt,bank);
    else for(uint32_t i=0u;i<32u;i++) {
        dispatch(g,&t->coarse_sor,(tiles+63u)/64u,1u,dt,bank);dispatch(g,&t->coarse_sor,(tiles+63u)/64u,1u,dt,bank|2u);
    }
    for(uint32_t reverse=3u;reverse>0u;reverse--) {
        uint32_t level=reverse-1u;multigrid_dispatch(g,&t->mg_prolong[level],dt,bank,level);multigrid_smooth(g,t,dt,bank,level,true);
    }
}
static bool multigrid_enabled(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t,float dt,uint32_t bank) {
    const char *setting=getenv("DC_GAS_MULTIGRID");
    if(!t->practical||dt>gas_multigrid_max_interval||!setting||strcmp(setting,"0")==0||(bank&FR_GAS_COMPARE_ROW_SETUP_BIT)!=0u) return false;
    uint64_t cells=0;
    for(uint32_t level=1u;level<=3u;level++) {uint32_t scale=1u<<level;cells+=(uint64_t)((g->width+scale-1u)/scale)*((g->height+scale-1u)/scale);}
    return cells*32u+(uint64_t)g->width*g->height*4u<=t->transport_work.bytes;
}
bool fr_gas_acoustic_step(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t,uint32_t gx,uint32_t gy,
    float dt,uint32_t bank,dc_gpu_address_buffer_t *stats,char *err,uint32_t cap) {
    bool trace=getenv("DC_GAS_TRACE")!=NULL&&g->timestamp_bits;
    const char *direct_first_setting=getenv("DC_GAS_DIRECT_FIRST_AXIS");
    bool direct_first=t->practical&&getenv("DC_GAS_FUSED_AXIS")&&direct_first_setting&&strcmp(direct_first_setting,"0")!=0;
    if(t->direct_first_axis!=direct_first) {t->matrix_valid=false;t->direct_first_axis=direct_first;}
    uint32_t tail_query=t->large_transport?14u:4u;
    bool matrix_large=t->large_transport||t->practical;
    if(t->matrix_valid&&t->matrix_large!=matrix_large) t->matrix_valid=false;
    uint32_t sweeps=budget("DC_GAS_PRESSURE_SWEEPS",GAS_PRESSURE_SWEEPS),group=budget("DC_GAS_PRESSURE_GROUP",GAS_PRESSURE_GROUP);
    bool compact_large=t->large_transport&&enabled("DC_GAS_SPARSE_LARGE");
    bool sparse=enabled("DC_GAS_SPARSE_RHS")&&t->matrix_valid&&(!t->large_transport||compact_large);
    bool sparse_activity=enabled("DC_GAS_SPARSE_ACTIVITY")&&t->matrix_valid&&!t->geometry_changed&&(!t->large_transport||compact_large);
    if(t->geometry_changed) bank|=FR_GAS_GEOMETRY_WAKE_BIT;
    bool sparse_classify=enabled("DC_GAS_SPARSE_CLASSIFY");
    const char *empty_setting=getenv("DC_GAS_SKIP_EMPTY_PRESSURE");
    bool skip_empty=t->practical&&empty_setting&&strcmp(empty_setting,"0")!=0;
    if(skip_empty&&!t->matrix_valid) sparse_classify=false;
    if(sparse) bank|=256u;
    if(sparse_activity) bank|=512u;
    if(sparse_classify) bank|=1024u;
    if(t->large_transport&&!compact_large) bank|=2048u;
    if(matrix_large) bank|=FR_GAS_LARGE_ENERGY_BIT;
    if(t->practical) bank|=FR_GAS_PRACTICAL_BIT;
    if(skip_empty) bank|=FR_GAS_SKIP_EMPTY_PRESSURE_BIT;
    if(getenv("DC_GAS_COMPARE_ROW_SETUP")) bank|=FR_GAS_COMPARE_ROW_SETUP_BIT;
    const char *zero_energy=getenv("DC_GAS_ZERO_ENERGY");
    if(t->practical&&zero_energy&&strcmp(zero_energy,"0")!=0&&(bank&FR_GAS_COMPARE_ROW_SETUP_BIT)==0u) bank|=FR_GAS_ZERO_ENERGY_BIT;
    if(multigrid_enabled(g,t,dt,bank)) bank|=FR_GAS_MULTIGRID_BIT;
    const char *boundary_setting=getenv("DC_GAS_MG_BOUNDARY");
    if((bank&FR_GAS_MULTIGRID_BIT)!=0u&&boundary_setting&&strcmp(boundary_setting,"0")!=0) bank|=FR_GAS_MG_BOUNDARY_BIT;
    const char *jacobi_setting=getenv("DC_GAS_MG_JACOBI");
    if((bank&FR_GAS_MULTIGRID_BIT)!=0u&&(bank&FR_GAS_MG_BOUNDARY_BIT)==0u&&t->fine_jacobi.pipeline&&
       jacobi_setting&&strcmp(jacobi_setting,"0")!=0) bank|=FR_GAS_MG_JACOBI_BIT;
    const char *metadata_setting=getenv("DC_GAS_FUSED_METADATA");
    if(t->large_transport&&t->practical&&metadata_setting&&strcmp(metadata_setting,"0")!=0&&
       (bank&FR_GAS_COMPARE_ROW_SETUP_BIT)==0u) bank|=FR_GAS_FUSED_METADATA_BIT;
    const char *lean_setting=getenv("DC_GAS_LEAN_METADATA");
    if((bank&FR_GAS_FUSED_METADATA_BIT)!=0u&&lean_setting&&strcmp(lean_setting,"0")!=0) bank|=FR_GAS_LEAN_METADATA_BIT;
    const char *packed_setting=getenv("DC_GAS_PACKED_PRESSURE"),*temporal_setting=getenv("DC_GAS_TEMPORAL_SOR");
    if(t->practical&&t->packed_sor.pipeline&&packed_setting&&strcmp(packed_setting,"0")!=0&&
       temporal_setting&&strcmp(temporal_setting,"0")!=0&&(bank&FR_GAS_COMPARE_ROW_SETUP_BIT)==0u) bank|=FR_GAS_PACKED_PRESSURE_BIT;
    const char *half_setting=getenv("DC_GAS_HALF_PRESSURE");
    if((bank&FR_GAS_MULTIGRID_BIT)!=0u&&(bank&FR_GAS_MG_BOUNDARY_BIT)==0u&&t->half_sor.pipeline&&half_setting&&strcmp(half_setting,"0")!=0&&
       temporal_setting&&strcmp(temporal_setting,"0")!=0&&(bank&FR_GAS_COMPARE_ROW_SETUP_BIT)==0u) bank=(bank&~FR_GAS_PACKED_PRESSURE_BIT)|FR_GAS_HALF_PRESSURE_BIT;
    if(!t->matrix_valid) bank|=4u;
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin acoustic advection");
    if(trace) {vkCmdResetQueryPool(g->command,g->timestamp_pool,0,tail_query+2u);vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,0);}
    fr_gas_gpu_record_pending_geometry(g);
    if(sparse) indirect_dispatch(g,t,&t->clear_pressure,dt,bank,48);
    barrier(g);
    vkCmdFillBuffer(g->command,t->indirect.buffer,32,4,0);vkCmdFillBuffer(g->command,t->indirect.buffer,48,4,0);
    uint32_t tiles=((g->width+7u)/8u)*((g->height+7u)/8u);
    const char *cached_neighbors=getenv("DC_GAS_CACHED_NEIGHBORS");
    const fr_renderer_compute_t *activity=t->practical&&cached_neighbors&&strcmp(cached_neighbors,"0")!=0&&
        (bank&FR_GAS_COMPARE_ROW_SETUP_BIT)==0u?&t->activity_cached:&t->activity;
    if(sparse_activity) {
        barrier(g);vkCmdFillBuffer(g->command,t->fields[9].buffer,0,tiles*4u,0);
        vkCmdFillBuffer(g->command,t->indirect.buffer,0,4,0);
        dispatch(g,&t->activity_candidates,(tiles+63u)/64u,1u,dt,bank);
        indirect_dispatch(g,t,activity,dt,bank,0);
    } else if(!t->large_transport||compact_large) dispatch(g,activity,(g->width+7u)/8u,(g->height+7u)/8u,dt,bank);
    dispatch(g,&t->activity_compact,(tiles+63u)/64u,1u,dt,bank);
    if(t->large_transport) {
        const char *reuse_rate=getenv("DC_GAS_REUSE_LINE_RATE");
        if(t->practical&&reuse_rate&&strcmp(reuse_rate,"0")!=0) bank|=FR_GAS_REUSE_LINE_RATE_BIT;
        const char *fused_setting=getenv("DC_GAS_FUSED_APPLY");bool fused=fused_setting&&strcmp(fused_setting,"0")!=0;
        barrier(g);VkBufferCopy copy={.size=t->state_bytes};
        if(!direct_first||!t->matrix_valid) vkCmdCopyBuffer(g->command,t->state_bank[bank&1u],t->state_bank[(bank&1u)^1u],1,&copy);
        if(trace) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,4);
        for(uint32_t axis=0;axis<2;axis++) {
            uint32_t flags=bank|(axis<<3u)|(direct_first?FR_GAS_DIRECT_FIRST_AXIS_BIT:0u),lines=axis==0?g->height:g->width;
            if(getenv("DC_GAS_STAGED_RATES")) {
                flags|=FR_GAS_STAGED_RATES_BIT;
                dispatch(g,&t->stage_transport,gx,gy,dt,flags);
            }
            const char *hybrid_setting=getenv("DC_GAS_HYBRID_LINES");
            bool hybrid=t->practical&&t->practical_explicit.pipeline&&hybrid_setting&&strcmp(hybrid_setting,"0")!=0&&
                (bank&FR_GAS_COMPARE_ROW_SETUP_BIT)==0u;
            const char *fused_hybrid_setting=getenv("DC_GAS_FUSED_HYBRID");
            bool fused_hybrid=hybrid&&getenv("DC_GAS_FUSED_AXIS")&&t->practical_hybrid.pipeline&&
                fused_hybrid_setting&&strcmp(fused_hybrid_setting,"0")!=0;
            if(fused_hybrid) flags|=FR_GAS_HYBRID_LINES_BIT;
            if(!fused_hybrid&&t->practical&&t->practical_explicit.pipeline&&hybrid_setting&&strcmp(hybrid_setting,"0")!=0&&
               (bank&FR_GAS_COMPARE_ROW_SETUP_BIT)==0u) {
                flags|=FR_GAS_HYBRID_LINES_BIT;dispatch(g,&t->practical_explicit,lines,1u,dt,flags);
            }
            if(t->practical&&getenv("DC_GAS_FUSED_AXIS")) {
                const char *fixed_axis_setting=getenv("DC_GAS_FIXED_AXIS");
                bool fixed_axis=fused_hybrid&&fixed_axis_setting&&strcmp(fixed_axis_setting,"0")!=0;
                const fr_renderer_compute_t *hybrid=fixed_axis?&t->practical_hybrid_axis[axis]:&t->practical_hybrid;
                dispatch(g,fused_hybrid?hybrid:&t->practical_axis,lines,1u,dt,flags);
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
        if((bank&FR_GAS_FUSED_METADATA_BIT)==0u) indirect_dispatch(g,t,&t->transport_metadata,dt,bank,32);
        else if((bank&FR_GAS_LEAN_METADATA_BIT)!=0u) indirect_dispatch(g,t,&t->metadata_lean,dt,bank,32);
        if(trace) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,13);
    } else indirect_dispatch(g,t,&t->advect,dt,bank,32);
    const fr_renderer_compute_t *rhs=(bank&FR_GAS_LEAN_METADATA_BIT)!=0u?&t->rhs_lean:((bank&FR_GAS_FUSED_METADATA_BIT)!=0u?&t->rhs_fused:&t->rhs);
    if(sparse&&(bank&FR_GAS_COMPARE_ROW_SETUP_BIT)==0u) indirect_dispatch(g,t,rhs,dt,bank,32);
    else dispatch(g,rhs,gx,gy,dt,bank&~256u);
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
            if(skip_empty) dispatch(g,&t->reset_pressure_mask,(tiles+63u)/64u,1u,dt,bank);
            else {barrier(g);vkCmdFillBuffer(g->command,t->fields[6].buffer,0,tiles*4u,0);}
            vkCmdFillBuffer(g->command,t->indirect.buffer,48,4,0);
            dispatch(g,&t->final_compact,(tiles+63u)/64u,1u,dt,bank);
            indirect_dispatch(g,t,&t->classify,dt,bank,48);
        } else dispatch(g,&t->classify,(g->width+7u)/8u,(g->height+7u)/8u,dt,bank);
        dispatch(g,&t->compact,(tiles+63u)/64u,1u,dt,bank);
        if(sparse) indirect_dispatch(g,t,&t->prepare_pressure,dt,bank,16);
        if((bank&FR_GAS_PACKED_PRESSURE_BIT)!=0u) indirect_dispatch(g,t,&t->pack_pressure,dt,bank,0);
        if((bank&FR_GAS_HALF_PRESSURE_BIT)!=0u) indirect_dispatch(g,t,&t->pack_half_pressure,dt,bank,0);
        bool multigrid=(bank&FR_GAS_MULTIGRID_BIT)!=0u;
        if(multigrid) multigrid_cycle(g,t,dt,bank,tiles);
        else if(coarse) {
            pressure_sweeps(g,t,dt,bank,16u);
            dispatch(g,&t->restrict_pressure,tiles,1u,dt,bank);
            const char *coarse_setting=getenv("DC_GAS_COARSE_WG");
            if(t->coarse_group.pipeline&&coarse_setting&&strcmp(coarse_setting,"0")!=0&&tiles<=FR_GAS_COARSE_GROUP_CAPACITY)
                dispatch(g,&t->coarse_group,1u,1u,dt,bank);
            else for(uint32_t sweep=0;sweep<32u;sweep++) {
                dispatch(g,&t->coarse_sor,(tiles+63u)/64u,1u,dt,bank);
                dispatch(g,&t->coarse_sor,(tiles+63u)/64u,1u,dt,bank|2u);
            }
            indirect_dispatch(g,t,&t->prolong_pressure,dt,bank,0);
        }
        if(!multigrid) pressure_sweeps(g,t,dt,bank,batch_sweeps);
        indirect_dispatch(g,t,&t->correct,dt,bank,0);
        if(trace&&batch+1u==batch_group) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,2);
        barrier(g);vkCmdFillBuffer(g->command,stats->buffer,FR_GAS_RESIDUAL_WORD*4u,4,0);
        if(!t->practical||trace) indirect_dispatch(g,t,&t->residual,dt,bank,16);
        }
        if(trace) vkCmdWriteTimestamp2(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,g->timestamp_pool,3);
        barrier(g);vkCmdFillBuffer(g->command,t->indirect.buffer,48,4,0);
        dispatch(g,&t->final_compact,(tiles+63u)/64u,1u,dt,bank);
        const char *group_rate_setting=getenv("DC_GAS_GROUP_RATE");
        uint32_t group_rate_index=group_rate_setting&&strcmp(group_rate_setting,"256")==0?1u:0u;
        if(!t->finish_group[group_rate_index].pipeline) group_rate_index=0u;
        if(t->practical&&group_rate_setting&&strcmp(group_rate_setting,"0")!=0&&
           (bank&FR_GAS_COMPARE_ROW_SETUP_BIT)==0u&&t->finish_group[group_rate_index].pipeline)
            dispatch(g,&t->finish_group[group_rate_index],(tiles+(2u<<group_rate_index)-1u)/(2u<<group_rate_index),1u,dt,bank);
        else indirect_dispatch(g,t,&t->finish,dt,bank,48);
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
