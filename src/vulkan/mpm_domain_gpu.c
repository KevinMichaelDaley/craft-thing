#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "liquid_water_domain_gpu.h"
#include "reactive_storage_gpu.h"
#include "dungeoncraft/reactive_arena_layout.h"

enum {
    CELL=1, PAGE=2, OCC=3, DIRTY=4, SOURCE=5, NEXT_SOURCE=6, TILE=7, FLOW=10, PARTICLE=19,
    COUNT=20, PROPOSAL=21, OUTPUT=22, GRID=23, FORCE=24, SPEED=25, ACCEPT=26,
    WORK=27, LABEL=28, NEXT_LABEL=29, SIZE=30, BINDINGS=31
};

struct dc_mpm_domain {
    fr_renderer_compute_t mpm, component, parcels;
    dc_gpu_address_buffer_t storage;
    VkDescriptorBufferInfo field[BINDINGS];
    uint32_t width,height,groups;
};

void dc_gpu_grain_domain_destroy(dc_gpu_t *gpu, struct dc_mpm_domain *d) {
    if (!d) return;
    fr_renderer_compute_destroy(gpu,&d->mpm);
    fr_renderer_compute_destroy(gpu,&d->component);
    fr_renderer_compute_destroy(gpu,&d->parcels);
    dc_gpu_address_buffer_destroy(gpu,&d->storage);
    free(d);
}

static void *mapped(struct dc_mpm_domain *d,unsigned field) {
    return (unsigned char *)d->storage.mapped+d->field[field].offset;
}

static bool prepare(dc_gpu_t *gpu,char *err,uint32_t cap) {
    struct dc_gpu_seam_projection *water=gpu->seam_projection;
    if (water->grains) return true;
    struct dc_mpm_domain *d=calloc(1,sizeof(*d));
    if (!d) return false;
    d->width=water->width;
    d->height=water->height;
    d->groups=(d->width/16u)*(d->height/16u);
    VkDeviceSize grid=(VkDeviceSize)d->width*d->height,tiles=water->tiles;
    VkDeviceSize particles=tiles*DC_MPM_PARTICLES_PER_CHUNK;
    VkDeviceSize pool=particles*sizeof(dc_mpm_particle_t);
    const VkDeviceSize sizes[BINDINGS]={
        [CELL]=tiles*DC_CHUNK_CELLS*sizeof(dc_cell_t),[PAGE]=grid/DC_CHUNK_CELLS*4u,
        [OCC]=grid*8u,[DIRTY]=(tiles+1u)*4u,[SOURCE]=particles*4u,[NEXT_SOURCE]=particles*4u,[TILE]=tiles*8u,
        [FLOW]=grid*sizeof(dc_face_velocity_t),[PARTICLE]=pool,[COUNT]=tiles*4u,
        [PROPOSAL]=pool,[OUTPUT]=pool,[GRID]=grid*16u,[FORCE]=grid*16u,[SPEED]=grid*8u,
        [ACCEPT]=particles*4u,[WORK]=(16u+2u*d->groups)*sizeof(uint32_t),
        [LABEL]=grid*4u,[NEXT_LABEL]=grid*4u,[SIZE]=grid*4u};
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(gpu->physical,&properties);
    VkDeviceSize alignment=properties.limits.minStorageBufferOffsetAlignment,bytes=0;
    for (unsigned i=0;i<BINDINGS;i++) {
        if (sizes[i]>properties.limits.maxStorageBufferRange) goto fail;
        bytes=(bytes+alignment-1u)/alignment*alignment;
        d->field[i].offset=bytes;
        d->field[i].range=sizes[i]?sizes[i]:4u;
        bytes+=d->field[i].range;
    }
    if (!dc_gpu_address_buffer_create(gpu,bytes,true,&d->storage,err,cap)) goto fail;
    for (unsigned i=0;i<BINDINGS;i++) d->field[i].buffer=d->storage.buffer;
    if (!fr_renderer_compute_init(gpu,&d->mpm,d->field,BINDINGS,28u,
            "build/shaders/mpm_domain.comp.spv",err,cap) ||
        !fr_renderer_compute_init(gpu,&d->component,d->field,BINDINGS,28u,
            "build/shaders/mpm_component.comp.spv",err,cap)) goto fail;
    uint32_t *work=mapped(d,WORK);
    for (unsigned i=0;i<d->groups;i++) work[3u+i]=i;
    work[10u+2u*d->groups]=d->groups;
    water->grains=d;
    return true;
fail:
    dc_gpu_grain_domain_destroy(gpu,d);
    if (err && cap && !*err) snprintf(err,cap,"Cannot allocate shared granular domain");
    return false;
}

static void copy_field(VkCommandBuffer cmd,const struct dc_mpm_domain *d,unsigned field,
    VkBuffer owner,VkDeviceSize owner_at,VkDeviceSize domain_at,VkDeviceSize bytes,bool publish) {
    VkDeviceSize at=d->field[field].offset+domain_at;
    VkBufferCopy region={publish?at:owner_at,publish?owner_at:at,bytes};
    vkCmdCopyBuffer(cmd,publish?d->storage.buffer:owner,publish?owner:d->storage.buffer,1,&region);
}

static void copy_grid(VkCommandBuffer cmd,const struct dc_mpm_domain *d,unsigned field,
    VkBuffer owner,const dc_gpu_water_tile_t *l,uint32_t word_bytes,VkDeviceSize owner_base,
    VkDeviceSize domain_base,bool publish) {
    VkBufferCopy rows[DC_CHUNK_SIDE];
    for (uint32_t y=0;y<DC_CHUNK_SIDE;y++) {
        VkDeviceSize from=owner_base+((VkDeviceSize)(l->y+y)*l->width+l->x)*word_bytes;
        VkDeviceSize to=d->field[field].offset+domain_base+
            ((VkDeviceSize)(l->scratch_y+y)*d->width+l->scratch_x)*word_bytes;
        rows[y]=(VkBufferCopy){publish?to:from,publish?from:to,DC_CHUNK_SIDE*word_bytes};
    }
    vkCmdCopyBuffer(cmd,publish?d->storage.buffer:owner,publish?owner:d->storage.buffer,DC_CHUNK_SIDE,rows);
}

static void transfer_tile(VkCommandBuffer cmd,const struct dc_mpm_domain *d,dc_gpu_t *owner,
    const dc_gpu_water_tile_t *l,uint32_t tile,bool peer_bodies,bool publish) {
    const VkDeviceSize cells=DC_CHUNK_CELLS*sizeof(dc_cell_t);
    const VkDeviceSize pool=DC_MPM_PARTICLES_PER_CHUNK*sizeof(dc_mpm_particle_t);
    copy_field(cmd,d,CELL,owner->chunk_buffer,l->slot*cells,tile*cells,cells,publish);
    copy_field(cmd,d,COUNT,owner->particle_count_buffer,l->slot*4u,tile*4u,4u,publish);
    copy_field(cmd,d,PARTICLE,owner->particle_buffer,l->slot*pool,tile*pool,pool,publish);
    if (publish && owner->mpm_swap_buffers)
        copy_field(cmd,d,PARTICLE,owner->mpm_output_buffer,l->slot*pool,tile*pool,pool,true);
    copy_grid(cmd,d,FLOW,owner->velocity_buffer,l,sizeof(dc_face_velocity_t),0,0,publish);
    if (publish) {
        copy_grid(cmd,d,GRID,owner->mpm_grid_buffer,l,16u,0u,0u,true);
        copy_grid(cmd,d,FORCE,owner->mpm_force_buffer,l,16u,0u,0u,true);
        copy_grid(cmd,d,SPEED,owner->mpm_velocity_buffer,l,8u,0u,0u,true);
    } else {
        copy_grid(cmd,d,OCC,owner->occupancy_buffer,l,4u,0u,0u,false);
        if (peer_bodies)
            copy_grid(cmd,d,OCC,owner->occupancy_buffer,l,4u,
                (VkDeviceSize)owner->width*owner->height*4u*DC_HYDRO_MPM_OCCUPANCY_PLANE,
                (VkDeviceSize)d->width*d->height*4u,false);
        if (owner->gas) copy_grid(cmd,d,FORCE,owner->mpm_force_buffer,l,16u,0u,0u,false);
    }
}

static void dispatch(dc_gpu_t *gpu,const struct dc_mpm_domain *d,const fr_renderer_compute_t *c,
    uint32_t mode,bool gas) {
    uint32_t push[]={d->width,d->height,mode,0u,gas,d->width*d->height,0u};
    vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,c->pipeline);
    vkCmdBindDescriptorSets(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,c->pipeline_layout,0,1,&c->descriptor,0,NULL);
    vkCmdPushConstants(gpu->command,c->pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
    vkCmdDispatch(gpu->command,d->width/16u,d->height/16u,1u);
    fr_renderer_barrier(gpu->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
}

static bool prepare_parcels(dc_gpu_t *gpu,struct dc_mpm_domain *d,dc_gpu_t *owners[2],
                            char *err,uint32_t cap) {
    uint32_t live[2]={0};
    for(unsigned i=0;i<2;i++) {
        if (!fr_gpu_reactive_initialize(owners[i],err,cap) ||
            !fr_gpu_reactive_particle_storage(owners[i],err,cap)) return false;
        live[i]=((uint32_t *)owners[i]->reactive->arena.mailbox.mapped)[FR_ARENA_MAIL_LIVE];
        uint32_t grains=0;
        for (uint32_t slot=0;slot<dc_gpu_slot_capacity(owners[i]);slot++)
            grains+=((const uint32_t *)owners[i]->particle_count_mapped)[slot];
        if (grains<live[i]) live[i]=grains;
    }
    for(unsigned i=0;i<2;i++)
        if(!fr_reactive_arena_reserve_free(owners[i],&owners[i]->reactive->arena,live[1u-i],err,cap))
            return false;
    VkDescriptorBufferInfo fields[]={d->field[SOURCE],d->field[TILE],d->field[DIRTY]};
    return d->parcels.pipeline || fr_renderer_compute_init(gpu,&d->parcels,fields,3u,20u,
        "build/shaders/mpm_parcel_domain.comp.spv",err,cap);
}

static void transfer_parcels(dc_gpu_t *gpu,struct dc_mpm_domain *d,dc_gpu_t *owners[2],
                             const dc_gpu_water_tile_t *locations,uint32_t count) {
    uint32_t push[5];
    for(unsigned i=0;i<2;i++) memcpy(push+2u*i,&owners[i]->reactive->arena.root.address,8u);
    vkCmdBindPipeline(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,d->parcels.pipeline);
    vkCmdBindDescriptorSets(gpu->command,VK_PIPELINE_BIND_POINT_COMPUTE,d->parcels.pipeline_layout,
        0,1,&d->parcels.descriptor,0,NULL);
    for(unsigned phase=0;phase<2;phase++) {
        push[4]=phase;
        vkCmdPushConstants(gpu->command,d->parcels.pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),push);
        vkCmdDispatch(gpu->command,DC_MPM_PARTICLES_PER_CHUNK/64u,count,1u);
        fr_renderer_barrier(gpu->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT|VK_ACCESS_2_TRANSFER_READ_BIT);
    }
    for(unsigned i=0;i<count;i++) {
        dc_gpu_address_buffer_t *banks=&owners[locations[i].context]->reactive->particles;
        VkDeviceSize bytes=DC_MPM_PARTICLES_PER_CHUNK*sizeof(fr_reactive_handle_t);
        VkDeviceSize at=locations[i].slot*bytes;
        VkBufferCopy copy={banks->bytes/2u+at,at,bytes};
        vkCmdCopyBuffer(gpu->command,banks->buffer,banks->buffer,1u,&copy);
    }
}

bool dc_gpu_run_grain_domain(dc_gpu_t *gpu,const dc_liquid_seam_endpoint_t *tiles,
    const dc_gpu_water_tile_t *locations,uint32_t count,char *err,uint32_t cap) {
    if (!prepare(gpu,err,cap)) return false;
    struct dc_mpm_domain *d=gpu->seam_projection->grains;
    dc_gpu_t *owners[2]={gpu,NULL};
    for (unsigned i=0;i<count;i++) owners[locations[i].context]=tiles[i].gpu;
    bool parcels=false;
    for(unsigned i=0;i<2;i++) parcels |= owners[i]->reactive && owners[i]->reactive->particles.buffer;
    if(parcels && !prepare_parcels(gpu,d,owners,err,cap)) return false;
    memset(mapped(d,PAGE),0,d->field[PAGE].range);
    memset(mapped(d,DIRTY),0,d->field[DIRTY].range);
    memset(mapped(d,OCC),0,d->field[OCC].range);
    memset(mapped(d,FORCE),0,d->field[FORCE].range);
    if (!fr_renderer_begin(gpu)) return false;
    fr_renderer_barrier(gpu->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
    for (uint32_t i=0;i<count;i++) {
        const dc_gpu_water_tile_t *l=locations+i;
        ((uint32_t *)mapped(d,PAGE))[(l->scratch_y/64u)*(d->width/64u)+l->scratch_x/64u]=i+1u;
        ((uint32_t *)mapped(d,TILE))[2u*i]=l->context;
        ((uint32_t *)mapped(d,TILE))[2u*i+1u]=l->slot;
        transfer_tile(gpu->command,d,tiles[i].gpu,l,i,owners[1u-l->context]->body_count!=0u,false);
    }
    fr_renderer_barrier(gpu->command,VK_PIPELINE_STAGE_2_TRANSFER_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT|VK_ACCESS_2_HOST_WRITE_BIT,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_READ_BIT|VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
    dispatch(gpu,d,&d->component,0u,false);
    for (uint64_t cells=(uint64_t)d->width*d->height;cells;cells/=2u)
        for (unsigned mode=1;mode<=3;mode++) dispatch(gpu,d,&d->component,mode,false);
    dispatch(gpu,d,&d->component,4u,false);
    dispatch(gpu,d,&d->mpm,7u,false);
    for (unsigned step=0;step<2;step++)
        for (unsigned mode=0;mode<6;mode++) {
            dispatch(gpu,d,&d->mpm,mode,!step&&(owners[0]->gas||owners[1]->gas));
            if (mode==1u) dispatch(gpu,d,&d->mpm,6u,false);
        }
    fr_renderer_barrier(gpu->command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_READ_BIT);
    if (parcels) transfer_parcels(gpu,d,owners,locations,count);
    for (unsigned i=0;i<count;i++) transfer_tile(gpu->command,d,tiles[i].gpu,locations+i,i,false,true);
    fr_renderer_barrier(gpu->command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_HOST_READ_BIT);
    if (!fr_renderer_submit(gpu)) return false;
    if (parcels) {
        owners[0]->bulk_phase_work_possible |= owners[1]->bulk_phase_work_possible;
        owners[1]->bulk_phase_work_possible = owners[0]->bulk_phase_work_possible;
        uint64_t high=owners[0]->reactive->high_id;
        if (owners[1]->reactive->high_id>high) high=owners[1]->reactive->high_id;
        owners[0]->reactive->high_id=owners[1]->reactive->high_id=high;
        if (((uint32_t *)mapped(d,DIRTY))[count]) {
            if (err && cap) snprintf(err,cap,"Cannot confirm paired reactive particle ownership");
            return false;
        }
    }
    for (unsigned i=0;i<count;i++)
        if (((uint32_t *)mapped(d,DIRTY))[i]) dc_gpu_structural_data(tiles[i].gpu)[DC_STRUCTURAL_COMPONENT_DIRTY_WORD]=1u;
    return true;
}
