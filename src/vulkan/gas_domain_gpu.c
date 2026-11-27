#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dungeoncraft/thermal_water_layout.h"
#include "ferrum/fire/combustion_gpu.h"
#include "gas_gpu_state.h"
#include "liquid_water_domain_gpu.h"
#include "thermal_gpu_internal.h"

enum { PAGE,CELL,OCC,GRID,FORCE,WORK,COLOR,BULK,CONTROL,HEAT,DISPLAY_FIELD,FIELD_COUNT };
struct dc_gas_domain {
    dc_gpu_t solver;
    dc_gpu_address_buffer_t fields[FIELD_COUNT];
};

bool dc_gpu_gas_owners_aligned(const dc_gpu_t *first,const dc_gpu_t *second) {
    const struct fr_gas_gpu *a=first->gas,*b=second->gas;
    if (!a || !b) return a==b;
    const float *pa=a->parameters.mapped,*pb=b->parameters.mapped;
    return a->policy==b->policy && a->density==b->density && a->temperature==b->temperature &&
        !memcmp(&a->model,&b->model,sizeof(a->model)) && a->clock.physical_steps==b->clock.physical_steps &&
        a->clock.advanced_seconds==b->clock.advanced_seconds && pa[FR_GAS_GRAVITY_WORD]==pb[FR_GAS_GRAVITY_WORD] &&
        pa[FR_GAS_WIND_WORD]==pb[FR_GAS_WIND_WORD] &&
        fr_combustion_gpu_enabled(first)==fr_combustion_gpu_enabled(second);
}

void dc_gpu_gas_domain_destroy(struct dc_gas_domain *d) {
    if (!d) return;
    fr_gas_gpu_destroy(&d->solver);
    for (unsigned i=0;i<FIELD_COUNT;i++) dc_gpu_address_buffer_destroy(&d->solver,&d->fields[i]);
    free(d);
}

static bool prepare(dc_gpu_t *owner,char *err,uint32_t cap) {
    struct dc_gpu_seam_projection *water=owner->seam_projection;
    if (water->gas) return true;
    struct dc_gas_domain *d=calloc(1,sizeof(*d));
    if (!d) return false;
    dc_gpu_t *g=&d->solver;
    g->device=owner->device; g->physical=owner->physical; g->queue=owner->queue;
    g->command=owner->command; g->pipeline_cache=owner->pipeline_cache;
    g->width=g->view_width=water->width; g->height=g->view_height=water->height;
    g->page_width=g->width/64u; g->page_height=g->height/64u; g->workspace_slots=water->tiles;
    VkDeviceSize grid=(VkDeviceSize)g->width*g->height,atlas=(VkDeviceSize)water->tiles*DC_CHUNK_CELLS;
    uint32_t groups=(g->width/16u)*(g->height/16u);
    const VkDeviceSize sizes[FIELD_COUNT]={
        [PAGE]=grid/DC_CHUNK_CELLS*sizeof(uint32_t),
        [CELL]=atlas*sizeof(dc_cell_t),
        [OCC]=grid*OCCUPANCY_PLANES*sizeof(uint32_t),
        [GRID]=grid*4u*sizeof(uint32_t),
        [FORCE]=grid*4u*sizeof(uint32_t),
        [WORK]=(3u+groups)*sizeof(uint32_t),
        [COLOR]=atlas*4u*sizeof(float),
        [BULK]=water->tiles*sizeof(fr_bulk_phase_chunk_t),
        [CONTROL]=DC_HYDRO_CONTROL_BYTES,
        [HEAT]=atlas*sizeof(float),
        [DISPLAY_FIELD]=grid*sizeof(uint32_t)
    };
    for (unsigned i=0;i<FIELD_COUNT;i++)
        if (!dc_gpu_address_buffer_create(g,sizes[i],i==PAGE||i==WORK||i==CONTROL,&d->fields[i],err,cap))
            goto fail;
    g->page_buffer=d->fields[PAGE].buffer; g->page_mapped=d->fields[PAGE].mapped;
    g->chunk_buffer=d->fields[CELL].buffer; g->occupancy_buffer=d->fields[OCC].buffer;
    g->mpm_grid_buffer=d->fields[GRID].buffer; g->mpm_activity_buffer=d->fields[WORK].buffer;
    g->mpm_force_buffer=d->fields[FORCE].buffer; g->water_color_buffer=d->fields[COLOR].buffer;
    g->bulk_phase_buffer=d->fields[BULK].buffer; g->thermal_control_buffer=d->fields[CONTROL].buffer;
    g->cells=d->fields[DISPLAY_FIELD].buffer;
    uint32_t *work=d->fields[WORK].mapped;
    work[0]=groups>65535u?65535u:groups; work[1]=(groups+65534u)/65535u; work[2]=1u;
    for (unsigned i=0;i<groups;i++) work[3u+i]=i;
    if (!fr_gas_gpu_enable(g,&owner->gas->model,owner->gas->density,owner->gas->temperature,err,cap)) goto fail;
    water->gas=d;
    return true;
fail:
    dc_gpu_gas_domain_destroy(d);
    return false;
}

static void copy_slot(VkCommandBuffer cmd,VkBuffer source,VkBuffer domain,uint32_t slot,uint32_t tile,
                      VkDeviceSize bytes,bool publish) {
    VkBufferCopy copy={bytes*(publish?tile:slot),bytes*(publish?slot:tile),bytes};
    vkCmdCopyBuffer(cmd,publish?domain:source,publish?source:domain,1,&copy);
}
static void copy_rows(dc_gpu_t *g,const dc_gpu_water_tile_t *tile,VkBuffer owner,VkBuffer domain,
                      uint32_t bytes,VkDeviceSize from_base,VkDeviceSize to_base,bool publish) {
    VkBufferCopy copies[DC_CHUNK_SIDE];
    for (unsigned y=0;y<DC_CHUNK_SIDE;y++) {
        VkDeviceSize a=from_base+((VkDeviceSize)(tile->y+y)*tile->width+tile->x)*bytes;
        VkDeviceSize b=to_base+((VkDeviceSize)(tile->scratch_y+y)*g->width+tile->scratch_x)*bytes;
        copies[y]=(VkBufferCopy){publish?b:a,publish?a:b,DC_CHUNK_SIDE*bytes};
    }
    vkCmdCopyBuffer(g->command,publish?domain:owner,publish?owner:domain,DC_CHUNK_SIDE,copies);
}
static void transfer(struct dc_gas_domain *d,dc_gpu_t *owner,const dc_gpu_water_tile_t *l,uint32_t tile,bool publish) {
    dc_gpu_t *g=&d->solver;
    VkCommandBuffer cmd=g->command;
    struct fr_gas_gpu *src=owner->gas,*dst=g->gas;
    copy_slot(cmd,src->state[src->direct_publish].buffer,dst->state[dst->direct_publish].buffer,
        l->slot,tile,DC_CHUNK_CELLS*sizeof(gas_packet_t),publish);
    copy_slot(cmd,src->gates.buffer,dst->gates.buffer,l->slot,tile,DC_CHUNK_CELLS*4u,publish);
    copy_slot(cmd,owner->chunk_buffer,g->chunk_buffer,l->slot,tile,DC_CHUNK_CELLS*sizeof(dc_cell_t),publish);
    copy_slot(cmd,owner->water_color_buffer,g->water_color_buffer,l->slot,tile,DC_CHUNK_CELLS*16u,publish);
    VkBuffer heat=fr_thermal_gpu_water_buffer(owner);
    if (heat) copy_slot(cmd,heat,d->fields[HEAT].buffer,l->slot,tile,DC_CHUNK_CELLS*4u,publish);
    copy_rows(g,l,owner->mpm_force_buffer,g->mpm_force_buffer,16u,0u,0u,publish);
    if (publish) return;
    copy_slot(cmd,owner->bulk_phase_buffer,g->bulk_phase_buffer,l->slot,tile,sizeof(fr_bulk_phase_chunk_t),false);
    copy_rows(g,l,owner->mpm_grid_buffer,g->mpm_grid_buffer,16u,0u,0u,false);
    for (uint32_t plane=0;plane<OCCUPANCY_PLANES;plane++)
        copy_rows(g,l,owner->occupancy_buffer,g->occupancy_buffer,4u,
            (VkDeviceSize)plane*owner->width*owner->height*4u,(VkDeviceSize)plane*g->width*g->height*4u,false);
}

static bool exchange(struct dc_gas_domain *d,const dc_liquid_seam_endpoint_t *tiles,
                     const dc_gpu_water_tile_t *locations,uint32_t count,bool publish) {
    dc_gpu_t *g=&d->solver;
    if (!fr_renderer_begin(g)) return false;
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
    for (unsigned i=0;i<count;i++) transfer(d,tiles[i].gpu,locations+i,i,publish);
    fr_renderer_barrier(g->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_HOST_READ_BIT);
    return fr_renderer_submit(g);
}

bool dc_gpu_run_gas_domain(dc_gpu_t *gpu,const dc_liquid_seam_endpoint_t *tiles,
    const dc_gpu_water_tile_t *locations,uint32_t count,float seconds,char *err,uint32_t cap) {
    if (!prepare(gpu,err,cap)) return false;
    struct dc_gas_domain *d=gpu->seam_projection->gas;
    dc_gpu_t *g=&d->solver,*owners[2]={gpu,NULL};
    memset(g->page_mapped,0,d->fields[PAGE].bytes);
    for (unsigned i=0;i<count;i++) {
        const dc_gpu_water_tile_t *l=locations+i;
        owners[l->context]=tiles[i].gpu;
        ((uint32_t *)g->page_mapped)[(l->scratch_y/64u)*g->page_width+l->scratch_x/64u]=i+1u;
    }
    const struct fr_gas_gpu *reference=gpu->gas;
    const float *params=reference->parameters.mapped;
    const float *current=g->gas->parameters.mapped;
    if (g->gas->policy!=reference->policy &&
        !fr_gas_gpu_set_pressure_policy(g,reference->policy,err,cap)) return false;
    if (current[FR_GAS_GRAVITY_WORD]!=params[FR_GAS_GRAVITY_WORD] &&
        !fr_gas_gpu_set_gravity(g,params[FR_GAS_GRAVITY_WORD],err,cap)) return false;
    if (current[FR_GAS_WIND_WORD]!=params[FR_GAS_WIND_WORD] &&
        !fr_gas_gpu_set_wind(g,params[FR_GAS_WIND_WORD],err,cap)) return false;
    uint32_t *control=d->fields[CONTROL].mapped;
    control[0]=fr_thermal_gpu_water_buffer(gpu)?DC_THERMAL_WATER_MAGIC:0u;
    memcpy(control+DC_THERMAL_WATER_ADDRESS_WORD,&d->fields[HEAT].address,sizeof(VkDeviceAddress));
    control[DC_THERMAL_WATER_COUNT_WORD]=count*DC_CHUNK_CELLS;
    for (unsigned i=0;i<2;i++) {
        if (!fr_gas_gpu_materialize_owners(owners[i],UINT32_MAX,true,err,cap)) return false;
    }
    if (!exchange(d,tiles,locations,count,false)) return false;
    g->gas->clock=reference->clock;
    g->gas->large_step_hint=reference->large_step_hint;
    fr_gas_gpu_invalidate_cache(g);
    g->gas->geometry_dirty=owners[0]->gas->geometry_dirty || owners[1]->gas->geometry_dirty;
    g->gas->geometry_dynamic=owners[0]->gas->geometry_dynamic || owners[1]->gas->geometry_dynamic ||
        owners[0]->fluid_work_possible || owners[1]->fluid_work_possible ||
        owners[0]->body_count || owners[1]->body_count;
    g->gas->geometry_routes_dirty=true;
    if (!fr_gas_gpu_step_with_conduction(g,seconds,!fr_combustion_gpu_enabled(gpu),err,cap) ||
        !fr_gas_gpu_materialize_owners(g,UINT32_MAX,true,err,cap) ||
        !exchange(d,tiles,locations,count,true)) return false;
    for (unsigned i=0;i<2;i++) {
        owners[i]->gas->clock=g->gas->clock;
        owners[i]->gas->large_step_hint=g->gas->large_step_hint;
        fr_gas_gpu_invalidate_cache(owners[i]);
        owners[i]->gas->geometry_dirty=g->gas->geometry_dirty;
        owners[i]->gas->geometry_dynamic=g->gas->geometry_dynamic;
        owners[i]->gas->geometry_routes_dirty=true;
    }
    return true;
}
