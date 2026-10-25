#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dungeoncraft/gas_layout.h"
#include "gas_acoustic_gpu.h"
static bool error(char *err,uint32_t cap,const char *message) {if(err&&cap) snprintf(err,cap,"%s",message);return false;}
void fr_gas_acoustic_destroy(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t) {
    fr_renderer_compute_destroy(g,&t->advect);fr_renderer_compute_destroy(g,&t->initialize);
    fr_renderer_compute_destroy(g,&t->rhs);fr_renderer_compute_destroy(g,&t->sor);
    fr_renderer_compute_destroy(g,&t->residual);fr_renderer_compute_destroy(g,&t->finish);
    for(uint32_t i=0;i<4;i++) dc_gpu_address_buffer_destroy(g,t->fields+i);
    memset(t,0,sizeof(*t));
}
bool fr_gas_acoustic_create(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t,const VkDescriptorBufferInfo source[11],char *err,uint32_t cap) {
    if(t->ready) return true;
    VkDescriptorBufferInfo buffers[15];memcpy(buffers,source,11*sizeof(*source));
    VkDeviceSize bytes=(VkDeviceSize)g->width*g->height*16u;
    for(uint32_t i=0;i<4;i++) {
        if(!dc_gpu_address_buffer_create(g,bytes,true,t->fields+i,err,cap)) goto fail;
        buffers[11+i]=(VkDescriptorBufferInfo){t->fields[i].buffer,0,bytes};
    }
    fr_renderer_compute_t *pipelines[]={&t->advect,&t->initialize,&t->rhs,&t->sor,&t->residual,&t->finish};
    const char *paths[]={"advect","acoustic_init","pressure_rhs","pressure_sor","pressure_residual","acoustic_finish"};
    for(uint32_t i=0;i<6;i++) {
        char path[128];snprintf(path,sizeof(path),"build/shaders/gas_%s.comp.spv",paths[i]);
        if(!fr_renderer_compute_init(g,pipelines[i],buffers,15,16,path,err,cap)) goto fail;
    }
    t->ready=true;return true;
fail:
    fr_gas_acoustic_destroy(g,t);return false;
}
static void barrier(dc_gpu_t *g) {
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
}
static void dispatch(dc_gpu_t *g,const fr_renderer_compute_t *pipeline,uint32_t gx,uint32_t gy,float dt,uint32_t bank) {
    uint32_t bits=0;memcpy(&bits,&dt,4);uint32_t push[4]={g->width,g->height,bits,bank};barrier(g);
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline->pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline->pipeline_layout,0,1,&pipeline->descriptor,0,NULL);
    vkCmdPushConstants(g->command,pipeline->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,gx,gy,1);
}
static bool submit(dc_gpu_t *g,char *err,uint32_t cap) {
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_HOST_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
    return fr_renderer_submit(g)||error(err,cap,"Cannot complete acoustic pressure work");
}
bool fr_gas_acoustic_step(dc_gpu_t *g,fr_gas_acoustic_gpu_t *t,uint32_t gx,uint32_t gy,
    float dt,uint32_t bank,dc_gpu_address_buffer_t *stats,char *err,uint32_t cap) {
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin acoustic advection");
    dispatch(g,&t->advect,gx,gy,dt,bank);dispatch(g,&t->initialize,gx,gy,dt,bank);dispatch(g,&t->rhs,gx,gy,dt,bank);
    uint32_t stalled=0,batches=0;float best=INFINITY;bool trace=getenv("DC_GAS_TRACE")!=NULL;
    for(;;) {
        for(uint32_t sweep=0;sweep<8;sweep++) {
            dispatch(g,&t->sor,gx,gy,dt,bank);dispatch(g,&t->sor,gx,gy,dt,bank|2u);
        }
        barrier(g);vkCmdFillBuffer(g->command,stats->buffer,FR_GAS_RESIDUAL_WORD*4u,4,0);
        dispatch(g,&t->residual,gx,gy,dt,bank);
        if(!submit(g,err,cap)) return false;
        uint32_t bits=((uint32_t *)stats->mapped)[FR_GAS_RESIDUAL_WORD];float residual=0;memcpy(&residual,&bits,4);
        batches++;if(trace&&(batches<=4u||(batches&(batches-1u))==0u))
            fprintf(stderr,"Acoustic dt%.9g batch%u residual%.9g error%u\n",dt,batches,residual,((uint32_t *)stats->mapped)[FR_GAS_ERROR_WORD]);
        if(((uint32_t *)stats->mapped)[FR_GAS_ERROR_WORD]) return true;
        if(residual<=1e-7f) break;
        if(residual<best*.99f) {best=residual;stalled=0;} else stalled++;
        if(!isfinite(residual)||stalled>=8) {((uint32_t *)stats->mapped)[FR_GAS_ERROR_WORD]=1;return true;}
        if(!fr_renderer_begin(g)) return error(err,cap,"Cannot continue acoustic pressure solve");
    }
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot finish acoustic pressure solve");
    dispatch(g,&t->finish,gx,gy,dt,bank);return submit(g,err,cap);
}
