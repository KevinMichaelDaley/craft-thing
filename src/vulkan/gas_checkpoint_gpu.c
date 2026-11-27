#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gas_gpu_state.h"
#include "thermal_gpu_internal.h"
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
bool fr_gas_gpu_materialize_owners(dc_gpu_t *g,uint32_t slot,bool collapse,char *err,uint32_t cap) {
    struct fr_gas_gpu *t=g->gas;if(!t->sparse_owners) return true;
    if(!fr_renderer_begin(g)) return error(err,cap,"Cannot begin cold logical gas gather");
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    uint32_t push[4]={g->width,g->height,collapse?1u:0u,slot};fr_renderer_compute_t *pipeline=&t->owner_gather;
    vkCmdBindPipeline(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline->pipeline);
    vkCmdBindDescriptorSets(g->command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline->pipeline_layout,0,1,&pipeline->descriptor,0,NULL);
    vkCmdPushConstants(g->command,pipeline->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(g->command,slot==UINT32_MAX?t->groups_x:64u,slot==UINT32_MAX?t->groups_y:1u,1u);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_HOST_READ_BIT);
    if(collapse) {VkBufferCopy copy={.size=t->state[0].bytes};vkCmdCopyBuffer(g->command,t->state[0].buffer,t->state[1].buffer,1,&copy);}
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_HOST_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    return fr_gas_submit(g)||error(err,cap,"Cannot complete cold logical gas gather");
}
bool fr_gas_gpu_state_memory(const dc_gpu_t *g,uint64_t *mapped,uint64_t *device,uint64_t *staging) {
    if(!g||!g->gas||!mapped||!device||!staging) return false;
    *mapped=*device=0;*staging=g->gas->checkpoint_staging.bytes;
    for(uint32_t i=0;i<3u;i++) {
        const dc_gpu_address_buffer_t *buffer=g->gas->state+i;
        if(buffer->host_visible) *mapped+=buffer->bytes;else *device+=buffer->bytes;
    }
    return true;
}
static bool copy_state_chunk(dc_gpu_t *g,uint32_t bank,uint32_t slot,bool upload,char *err,uint32_t cap) {
    struct fr_gas_gpu *t=g->gas;VkDeviceSize bytes=4096u*sizeof(gas_packet_t),offset=(VkDeviceSize)slot*bytes;
    if(!t->checkpoint_staging.mapped||t->checkpoint_staging.bytes<bytes||!fr_renderer_begin(g))
        return error(err,cap,"Cannot begin cold gas state staging");
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkBufferCopy copy={upload?0:offset,upload?offset:0,bytes};
    vkCmdCopyBuffer(g->command,upload?t->checkpoint_staging.buffer:t->state[bank].buffer,
        upload?t->state[bank].buffer:t->checkpoint_staging.buffer,1,&copy);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_HOST_BIT|VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_HOST_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_TRANSFER_READ_BIT);
    return fr_gas_submit(g)||error(err,cap,"Cannot complete cold gas state staging");
}
bool fr_gas_gpu_checkpoint_packets(dc_gpu_t *g,uint32_t bank,uint32_t slot,const gas_packet_t **packets,char *err,uint32_t cap) {
    if(!g||!g->gas||bank>=3u||slot>=DC_GPU_CHUNK_SLOTS||!packets) return error(err,cap,"Invalid cold gas state request");
    if(g->gas->sparse_owners&&bank==1u) {if(!fr_gas_gpu_materialize_owners(g,slot,false,err,cap)) return false;bank=0u;}
    if(g->gas->state[bank].mapped) {*packets=(const gas_packet_t *)g->gas->state[bank].mapped+(size_t)slot*4096u;return true;}
    if(!copy_state_chunk(g,bank,slot,false,err,cap)) return false;
    *packets=g->gas->checkpoint_staging.mapped;return true;
}
static bool write_chunk(dc_gpu_t *g,uint32_t slot,const void *packets,char *err,uint32_t cap) {
    struct fr_gas_gpu *t=g->gas;uint32_t bank=t->direct_publish?1u:0u;size_t bytes=4096u*sizeof(gas_packet_t);
    if(t->sparse_owners) {uint32_t *gates=(uint32_t *)t->gates.mapped+(size_t)slot*4096u;for(uint32_t i=0;i<4096u;i++) gates[i]&=~FR_GAS_CELL_OWNER_BIT;}
    if(t->state[bank].mapped) {
        memcpy((gas_packet_t *)t->state[bank].mapped+(size_t)slot*4096u,packets,bytes);
        return fr_combustion_clear_emission_chunk(g,slot,err,cap);
    }
    memcpy(t->checkpoint_staging.mapped,packets,bytes);
    return copy_state_chunk(g,bank,slot,true,err,cap)&&fr_combustion_clear_emission_chunk(g,slot,err,cap);
}
static fr_gas_state_t unpack(const gas_packet_t *packet) {
    const float *p=packet->words;fr_gas_state_t s={0};
    for(uint32_t i=0;i<5;i++) s.species[i]=p[i];
    s.species[FR_GAS_EXHAUST]+=p[FR_GAS_EXHAUST_TAIL_WORD];
    s.species[FR_GAS_SOOT]+=p[FR_GAS_SOOT_TAIL_WORD];
    s.energy=(double)p[5]+p[11]+p[14];s.momentum[0]=p[6];s.momentum[1]=p[7];
    double carrier=s.species[FR_GAS_EXHAUST]+s.species[FR_GAS_SOOT];
    for(uint32_t i=0;i<3;i++) {
        float pigment=p[8+i];
        /* A saturated float may round above the compensated carrier. */
        s.pigment[i]=pigment>carrier&&isfinite(pigment)&&pigment==(float)carrier?carrier:pigment;
    }
    return s;
}
static bool import_thermodynamics(const fr_gas_model_t *model,const fr_gas_state_t *state,
    double volume,fr_gas_thermo_t *thermo,char *err,uint32_t cap) {
    return fr_gas_thermodynamics(model,state,volume==0.?1.:volume,thermo,err,cap);
}
bool fr_gas_gpu_seed_chunk(dc_gpu_t *g,uint32_t slot,const fr_gas_state_t *states,
    const double *volumes,uint32_t count,char *err,uint32_t cap) {
    if(!g||!g->gas||slot>=DC_GPU_CHUNK_SLOTS||!states||!volumes||count!=4096u) return error(err,cap,"Invalid immutable gas import");
    gas_packet_t *packets=calloc(count,sizeof(*packets));if(!packets) return error(err,cap,"Cannot stage gas import");
    bool okay=true;
    for(uint32_t i=0;okay&&i<count;i++) {
        fr_gas_thermo_t t;okay=volumes[i]>=0&&volumes[i]<=1&&
            import_thermodynamics(&g->gas->model,states+i,volumes[i],&t,err,cap);
        if(!okay) break;
        float *p=packets[i].words;
        for(uint32_t k=0;k<5;k++) p[k]=(float)states[i].species[k];
        p[FR_GAS_EXHAUST_TAIL_WORD]=(float)(states[i].species[FR_GAS_EXHAUST]-p[FR_GAS_EXHAUST]);
        p[FR_GAS_SOOT_TAIL_WORD]=(float)(states[i].species[FR_GAS_SOOT]-p[FR_GAS_SOOT]);
        p[5]=(float)states[i].energy;double residual=states[i].energy-p[5];p[11]=(float)residual;p[14]=(float)(residual-p[11]);
        p[6]=(float)states[i].momentum[0];p[7]=(float)states[i].momentum[1];
        for(uint32_t k=0;k<3;k++) p[8+k]=(float)states[i].pigment[k];
        p[12]=p[13]=(float)volumes[i];fr_gas_state_t rounded=unpack(packets+i);
        okay=import_thermodynamics(&g->gas->model,&rounded,p[12],&t,err,cap);
    }
    if(okay) okay=vkQueueWaitIdle(g->queue)==VK_SUCCESS;
    if(okay) okay=write_chunk(g,slot,packets,err,cap);
    if(okay) {fr_gas_gpu_invalidate_cache(g);g->gas->large_step_hint=0;g->gas->smoke_coordinate_reset=true;}
    free(packets);return okay;
}
bool fr_gas_gpu_read_chunk(dc_gpu_t *g,uint32_t slot,fr_gas_state_t *states,uint32_t count,char *err,uint32_t cap) {
    if(!g||!g->gas||slot>=DC_GPU_CHUNK_SLOTS||!states||count<4096u) return error(err,cap,"Invalid cold gas snapshot");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot complete gas snapshot");
    const gas_packet_t *packets=NULL;if(!fr_gas_gpu_checkpoint_packets(g,g->gas->direct_publish?1u:0u,slot,&packets,err,cap)) return false;
    for(uint32_t i=0;i<4096u;i++) states[i]=unpack(packets+i);
    return true;
}
bool fr_gas_gpu_probe_cell(dc_gpu_t *g, uint32_t x, uint32_t y, fr_gas_gpu_probe_t *out,
                          char *err, uint32_t cap) {
    enum { GAS_CELL_VOLUME_WORD = 12, GAS_CELL_WORK_WORD = 15 };
    if (!g || !out || !g->page_mapped || x >= g->width || y >= g->height)
        return error(err, cap, "Invalid gas probe coordinates or destination");
    const uint32_t *pages = g->page_mapped;
    size_t tile = (size_t)(y / DC_CHUNK_SIDE) * g->page_width + x / DC_CHUNK_SIDE;
    uint32_t page = pages[tile];
    if (!page || page > DC_GPU_CHUNK_SLOTS)
        return error(err, cap, "Gas probe cell is not resident");
    if (!g->gas) return error(err, cap, "Gas probe requires an enabled gas field");
    if (vkQueueWaitIdle(g->queue) != VK_SUCCESS)
        return error(err, cap, "Cannot complete gas probe snapshot");
    const gas_packet_t *packets = NULL;
    if (!fr_gas_gpu_checkpoint_packets(g, g->gas->direct_publish ? 1u : 0u,
                                       page - 1u, &packets, err, cap)) return false;
    uint32_t cell = (y % DC_CHUNK_SIDE) * DC_CHUNK_SIDE + x % DC_CHUNK_SIDE;
    const gas_packet_t *packet = &packets[cell];
    fr_gas_gpu_probe_t sample = {.volume = packet->words[GAS_CELL_VOLUME_WORD],
        .work = packet->words[GAS_CELL_WORK_WORD]};
    if (!(sample.volume > 0. && sample.volume <= 1.) || !isfinite(sample.work))
        return error(err, cap, "Gas probe has invalid void volume or work");
    fr_gas_state_t state = unpack(packet);
    const fr_gas_model_t *model = &g->gas->model;
    if (!fr_gas_thermodynamics(model, &state, sample.volume, &sample.thermo, err, cap))
        return false;
    if (sample.thermo.mass == 0.) return error(err, cap, "Gas probe cell is empty");
    for (uint32_t i = 0; i < FR_GAS_SPECIES; ++i)
        sample.heat_capacity += state.species[i] * model->heat_capacity_cv[i];
    sample.internal_energy = sample.heat_capacity * sample.thermo.temperature_K;
    *out = sample;
    return true;
}
bool fr_gas_gpu_export_checkpoint(dc_gpu_t *g,uint32_t slot,fr_gas_checkpoint_chunk_t *out,char *err,uint32_t cap) {
    if(!g||!g->gas||slot>=DC_GPU_CHUNK_SLOTS||!out) return error(err,cap,"Invalid cold gas checkpoint export");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot complete gas checkpoint export");
    struct fr_gas_gpu *t=g->gas;
    const gas_packet_t *packets=NULL;if(!fr_gas_gpu_checkpoint_packets(g,t->direct_publish?1u:0u,slot,&packets,err,cap)) return false;
    for(uint32_t i=0;i<4096u;i++) {
        memcpy(out->cells[i],packets[i].words,sizeof(out->cells[i]));
        out->carrier_tail[i][0]=packets[i].words[FR_GAS_EXHAUST_TAIL_WORD];
        out->carrier_tail[i][1]=packets[i].words[FR_GAS_SOOT_TAIL_WORD];
    }
    const uint32_t *gates=(uint32_t *)t->gates.mapped+(size_t)slot*4096u;
    for(uint32_t i=0;i<4096u;i++) out->gates[i]=gates[i]&3u;
    out->clock=t->clock;return true;
}
static bool import_packet(const fr_gas_model_t *model,const fr_gas_checkpoint_chunk_t *input,
    uint32_t i,gas_packet_t *packet,char *err,uint32_t cap) {
    const float *p=input->cells[i],*tail=input->carrier_tail[i];
    for(uint32_t word=0;word<16u;word++) if(!isfinite(p[word])) return error(err,cap,"Nonfinite gas checkpoint");
    if(p[12]<0.f||p[12]>1.f||p[13]<0.f||p[13]>1.f||(input->gates[i]&~3u)!=0u)
        return error(err,cap,"Invalid gas checkpoint geometry or face valves");
    if(!isfinite(tail[0])||!isfinite(tail[1])||
       (float)((double)p[FR_GAS_EXHAUST]+tail[0])!=p[FR_GAS_EXHAUST]||
       (float)((double)p[FR_GAS_SOOT]+tail[1])!=p[FR_GAS_SOOT])
        return error(err,cap,"Invalid compensated gas carrier stock");
    memcpy(packet->words,p,sizeof(input->cells[i]));
    packet->words[FR_GAS_EXHAUST_TAIL_WORD]=tail[0];
    packet->words[FR_GAS_SOOT_TAIL_WORD]=tail[1];
    fr_gas_state_t state=unpack(packet);fr_gas_thermo_t thermo;
    return import_thermodynamics(model,&state,p[12],&thermo,err,cap);
}
bool fr_gas_gpu_import_checkpoint(dc_gpu_t *g,uint32_t slot,const fr_gas_checkpoint_chunk_t *input,char *err,uint32_t cap) {
    if(!g||!g->gas||slot>=DC_GPU_CHUNK_SLOTS||!input) return error(err,cap,"Invalid cold gas checkpoint import");
    struct fr_gas_gpu *t=g->gas;
    if(!isfinite(input->clock.advanced_seconds)||input->clock.advanced_seconds<0.||
       input->clock.substeps<input->clock.physical_steps||
       (input->clock.physical_steps==0u&&input->clock.advanced_seconds!=0.)) return error(err,cap,"Invalid gas checkpoint clock");
    gas_packet_t *packets=calloc(4096u,sizeof(*packets));
    if(!packets) return error(err,cap,"Cannot stage gas checkpoint import");
    bool okay=true;
    for(uint32_t i=0;okay&&i<4096u;i++) okay=import_packet(&t->model,input,i,packets+i,err,cap);
    if(okay&&vkQueueWaitIdle(g->queue)!=VK_SUCCESS) okay=error(err,cap,"Cannot complete gas checkpoint import");
    if(okay) okay=write_chunk(g,slot,packets,err,cap);
    free(packets);
    if(!okay) return false;
    memcpy((uint32_t *)t->gates.mapped+(size_t)slot*4096u,input->gates,sizeof(input->gates));
    ((uint32_t *)t->parameters.mapped)[FR_GAS_MODEL_WORDS+slot]=0u;
    if(input->clock.advanced_seconds>t->clock.advanced_seconds) t->clock=input->clock;
    fr_gas_gpu_invalidate_cache(g);t->large_step_hint=0;t->smoke_coordinate_reset=true;return true;
}
bool fr_gas_gpu_read_work_chunk(dc_gpu_t *g,uint32_t slot,double *work,uint32_t count,char *err,uint32_t cap) {
    if(!g||!g->gas||slot>=DC_GPU_CHUNK_SLOTS||!work||count<4096u) return error(err,cap,"Invalid gas work snapshot");
    if(vkQueueWaitIdle(g->queue)!=VK_SUCCESS) return error(err,cap,"Cannot complete gas work snapshot");
    const gas_packet_t *packets=NULL;if(!fr_gas_gpu_checkpoint_packets(g,g->gas->direct_publish?1u:0u,slot,&packets,err,cap)) return false;
    for(uint32_t i=0;i<4096u;i++) work[i]=packets[i].words[15];
    return true;
}
