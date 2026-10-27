#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/material_profile.h"
#include "dungeoncraft/material_density.h"
#include "reactive_checkpoint_gpu.h"
#include "reactive_gpu_internal.h"
#include "reactive_storage_gpu.h"
#include "runtime_body_internal.h"

static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
void fr_gpu_reactive_checkpoint_clear_pending(dc_gpu_t *g) {
    if(!g) return;
    for(uint32_t i=0;i<g->checkpoint_chunk_count;i++) {
        if(g->checkpoint_reactive) fr_reactive_chunk_release(g->checkpoint_reactive+i);
        if(g->checkpoint_reactive_handles) free(g->checkpoint_reactive_handles[i]);
    }
    free(g->checkpoint_reactive);g->checkpoint_reactive=NULL;
    free(g->checkpoint_reactive_handles);g->checkpoint_reactive_handles=NULL;
}
void fr_gpu_reactive_checkpoint_remove_pending(dc_gpu_t *g,uint32_t index,uint32_t last) {
    if(g->checkpoint_reactive) {
        fr_reactive_chunk_release(g->checkpoint_reactive+index);
        if(index!=last) {g->checkpoint_reactive[index]=g->checkpoint_reactive[last];g->checkpoint_reactive[last]=(fr_reactive_chunk_t){0};}
    }
    if(g->checkpoint_reactive_handles) {
        free(g->checkpoint_reactive_handles[index]);g->checkpoint_reactive_handles[index]=NULL;
        if(index!=last) {g->checkpoint_reactive_handles[index]=g->checkpoint_reactive_handles[last];g->checkpoint_reactive_handles[last]=NULL;}
    }
}
bool fr_gpu_reactive_checkpoint_preflight_chunk(dc_gpu_t *g,const dc_chunk_t *chunk,
    uint32_t index,char *err,uint32_t cap) {
    if(!g||!chunk||index>=g->checkpoint_chunk_count) return error(err,cap,"Invalid pending checkpoint chunk");
    if(!g->checkpoint_reactive) return true;
    return fr_reactive_chunk_valid(g->checkpoint_reactive+index,chunk,err,cap)&&
        fr_gpu_reactive_storage_stream(g,g->reactive,err,cap);
}
bool fr_gpu_reactive_checkpoint_bind_chunk(dc_gpu_t *g,uint32_t slot,uint32_t index,
    char *err,uint32_t cap) {
    if(!g||index>=g->checkpoint_chunk_count) return error(err,cap,"Invalid checkpoint binding index");
    if(!g->checkpoint_reactive) return true;
    return fr_gpu_reactive_bind_prepared_chunk(g,slot,g->checkpoint_reactive+index,
        g->checkpoint_reactive_handles?g->checkpoint_reactive_handles[index]:NULL,err,cap);
}
static bool copy_channel(const fr_reactive_chunk_t *source,fr_reactive_chunk_t *out) {
    *out=*source;out->attachments=NULL;
    if(source->count) {
        out->attachments=malloc((size_t)source->count*sizeof(*out->attachments));
        if(!out->attachments) return false;
        memcpy(out->attachments,source->attachments,(size_t)source->count*sizeof(*out->attachments));
    }
    return true;
}
bool fr_gpu_reactive_checkpoint_collect(dc_gpu_t *g,const dc_chunk_t *chunks,
    uint32_t count,fr_reactive_archive_t *out,char *err,uint32_t cap) {
    fr_reactive_archive_t archive={.chunk_count=count};
    if(!g||!out||(!chunks&&count)||!fr_gpu_reactive_export_bodies(g,&archive.bodies,
        &archive.body_count,&archive.next_id,err,cap)) return false;
    archive.chunks=count?calloc(count,sizeof(*archive.chunks)):NULL;
    if(count&&!archive.chunks) {fr_reactive_archive_release(&archive);return error(err,cap,"Cannot stage checkpoint chunk channels");}
    bool okay=true;
    for(uint32_t i=0;okay&&i<count;i++) {
        bool found=false;
        for(uint32_t slot=0;!found&&slot<DC_GPU_CHUNK_SLOTS;slot++) {
            const uint32_t *registry=dc_gpu_structural_data(g)+DC_STRUCTURAL_HEADER_WORDS+slot*8u;
            if(registry[5]&&memcmp(registry,&chunks[i].coord,sizeof(chunks[i].coord))==0) {
                okay=fr_gpu_reactive_export_chunk(g,slot,chunks+i,archive.chunks+i,err,cap);found=true;
            }
        }
        for(uint32_t old=0;!found&&old<g->checkpoint_chunk_count;old++)
            if(memcmp(&g->checkpoint_chunks[old].coord,&chunks[i].coord,sizeof(chunks[i].coord))==0) {
                if(g->checkpoint_reactive) okay=copy_channel(g->checkpoint_reactive+old,archive.chunks+i);
                else archive.chunks[i].next_id=1;
                found=true;
            }
        if(!found) okay=error(err,cap,"Checkpoint physical chunk has no immutable channel source");
        if(archive.chunks[i].next_id>archive.next_id) archive.next_id=archive.chunks[i].next_id;
    }
    if(!okay) {fr_reactive_archive_release(&archive);return false;}
    *out=archive;return true;
}
typedef struct {uint32_t id,slot;} identity_t;
typedef struct {uint32_t kind,word,nibble;} binding_t;
static int identity_order(const void *a,const void *b) {
    const identity_t *x=a,*y=b;return x->id<y->id?-1:x->id>y->id?1:0;
}
static bool body_binding(const fr_reactive_record_t *record,const identity_t *ids,uint32_t count,
    const dc_structural_store_payload_t *payload,const uint32_t *pixels,uint32_t words,binding_t *out) {
    identity_t key={.id=record->owner.key[0]};const identity_t *match=bsearch(&key,ids,count,sizeof(*ids),identity_order);
    if(!match) return false;
    const dc_gpu_body_record_t *body=payload->bodies+match->slot;
    if((!body->body.active&&!body->reserved[0])||!(body->piece_count&DC_GPU_PIXEL_BODY_FLAG)||
       payload->states[match->slot].owner.generation!=record->owner.key[1]||
       payload->states[match->slot].owner.runtime_id!=record->owner.key[0]) return false;
    uint32_t source=body->compound_reserved,x=record->owner.key[2],y=record->owner.key[3];
    if((uint64_t)source+8u>words) return false;
    const uint32_t *shape=pixels+source;
    if(x>=shape[6]||y>=shape[7]||(uint64_t)source+shape[0]>words) return false;
    uint64_t tile=(uint64_t)(y/16u)*shape[2]+x/16u;
    if(tile+8u>=shape[0]) return false;
    uint32_t offset=shape[8u+tile],cell=(y%16u)*16u+x%16u;
    uint64_t word=(uint64_t)offset+1u+cell/8u;
    if(!offset||word>=shape[0]) return false;
    uint32_t nibble=(shape[word]>>((cell%8u)*4u))&15u;
    const fr_reactive_profile_t *profile=fr_reactive_profile_for_body(nibble);
    if(!profile||profile->material!=record->state.material) return false;
    *out=(binding_t){FR_REACTIVE_OWNER_BODY,source+(uint32_t)word,cell%8u};return true;
}
typedef struct {double mass,x,y,moment;} checkpoint_mass_t;
static void checkpoint_mass_add(checkpoint_mass_t *out,double mass,double x,double y) {
    out->mass+=mass;out->x+=mass*x;out->y+=mass*y;out->moment+=mass*(x*x+y*y+1./6.);
}
bool fr_gpu_reactive_checkpoint_mass_valid(const fr_reactive_archive_t *archive,
    const dc_structural_store_payload_t *payload,const uint32_t *pixels,uint32_t words,char *err,uint32_t cap) {
    if(!payload||(payload->body_count&&!payload->bodies)||(words&&!pixels)) return error(err,cap,"Invalid reactive mass validation input");
    bool needed=false;for(uint32_t i=0;i<payload->body_count;i++) {
        const dc_gpu_body_record_t *body=payload->bodies+i;
        needed|=(body->body.active||body->reserved[0])&&(body->motion_flags&DC_GPU_BODY_REACTIVE_MASS)!=0;
    }
    if(!needed) return true;
    if(!archive) return error(err,cap,"Inventory-derived rigid mass has no reactive checkpoint channel");
    if(archive->body_count&&!archive->bodies) return error(err,cap,"Reactive mass channel has no parcel records");
    checkpoint_mass_t *mass=calloc(payload->body_count,sizeof(*mass));
    identity_t *ids=malloc((size_t)payload->body_count*sizeof(*ids));
    if(!mass||!ids) {free(mass);free(ids);return error(err,cap,"Cannot validate reactive body mass");}
    bool okay=true;
    for(uint32_t i=0;i<payload->body_count&&okay;i++) {
        const dc_gpu_body_record_t *body=payload->bodies+i;ids[i]=(identity_t){body->body.id,i};
        if((!body->body.active&&!body->reserved[0])||!(body->motion_flags&DC_GPU_BODY_REACTIVE_MASS)) continue;
        float density=0;
        okay=(body->piece_count&DC_GPU_PIXEL_BODY_FLAG)&&body->compound_reserved<words&&
            dc_gpu_pixel_geometry_valid(body,pixels+body->compound_reserved,words-body->compound_reserved,&density);
        if(!okay) break;
        const uint32_t *shape=pixels+body->compound_reserved;uint64_t directory=(uint64_t)shape[2]*shape[3];
        for(uint32_t tile=0;tile<shape[1];tile++) {
            uint64_t at=8u+directory+(uint64_t)tile*33u;uint32_t tx=shape[at]&65535u,ty=shape[at]>>16;
            for(uint32_t pixel=0;pixel<256;pixel++) {
                uint32_t material=(shape[at+1u+pixel/8u]>>((pixel%8u)*4u))&15u;if(!material) continue;
                checkpoint_mass_add(mass+i,DC_BODY_DENSITY(material),tx*16u+pixel%16u+.5,ty*16u+pixel/16u+.5);
            }
        }
    }
    if(okay) qsort(ids,payload->body_count,sizeof(*ids),identity_order);
    for(uint32_t i=0;i<archive->body_count&&okay;i++) {
        const fr_reactive_record_t *record=archive->bodies+i;identity_t key={.id=record->owner.key[0]};
        const identity_t *match=bsearch(&key,ids,payload->body_count,sizeof(*ids),identity_order);
        if(!match) {okay=false;break;}
        if(!(payload->bodies[match->slot].motion_flags&DC_GPU_BODY_REACTIVE_MASS)) continue;
        binding_t binding;okay=body_binding(record,ids,payload->body_count,payload,pixels,words,&binding);if(!okay) break;
        uint32_t material=(pixels[binding.word]>>(binding.nibble*4u))&15u;
        double delta=(double)fr_reactive_state_mass_fp(&record->state)/65536.-DC_BODY_DENSITY(material);
        checkpoint_mass_add(mass+match->slot,delta,record->owner.key[2]+.5,record->owner.key[3]+.5);
    }
    for(uint32_t i=0;i<payload->body_count&&okay;i++) {
        const dc_gpu_body_record_t *body=payload->bodies+i;
        if((!body->body.active&&!body->reserved[0])||!(body->motion_flags&DC_GPU_BODY_REACTIVE_MASS)) continue;
        checkpoint_mass_t value=mass[i];double x=value.x/value.mass,y=value.y/value.mass;
        double inertia=fmax(1e-12,value.moment-value.mass*(x*x+y*y));
        double center_tolerance=fmax(1e-4,fmax(body->body.width,body->body.height)*2e-6);
        okay=value.mass>0&&fabs(body->mass-value.mass)<=fmax(5e-5,value.mass*1e-5)&&
            fabs(body->center[0]-x)<=center_tolerance&&fabs(body->center[1]-y)<=center_tolerance&&
            fabs(body->inertia-inertia)<=fmax(.005,inertia*2e-4);
    }
    free(mass);free(ids);return okay||error(err,cap,"Rigid mass or inertia disagrees with authoritative parcel inventory");
}
bool fr_gpu_reactive_checkpoint_prepare(dc_gpu_t *g,const fr_reactive_archive_t *archive,
    const dc_chunk_t *chunks,const dc_structural_store_payload_t *payload,const uint32_t *pixels,uint32_t pixel_words,
    struct fr_reactive_gpu **out,uint32_t ***out_handles,char *err,uint32_t cap) {
    if(!g||!payload||!out||!out_handles) return error(err,cap,"Invalid checkpoint reaction transaction");
    if(!fr_gpu_reactive_checkpoint_mass_valid(archive,payload,pixels,pixel_words,err,cap)) return false;
    if(!archive) {*out=NULL;*out_handles=NULL;return true;}
    if(archive->chunk_count&&!chunks) return error(err,cap,"Missing checkpoint physical chunks");
    uint32_t *slots=archive->chunk_count?malloc((size_t)archive->chunk_count*sizeof(*slots)):NULL;
    if(archive->chunk_count&&!slots) return error(err,cap,"Cannot preflight resident checkpoint channels");
    uint64_t total=archive->body_count;
    for(uint32_t i=0;i<archive->chunk_count;i++) {
        total+=archive->chunks[i].count;
        slots[i]=UINT32_MAX;
        for(uint32_t slot=0;slot<DC_GPU_CHUNK_SLOTS;slot++) {
            const uint32_t *registry=dc_gpu_structural_data(g)+DC_STRUCTURAL_HEADER_WORDS+slot*8u;
            if(registry[5]&&memcmp(registry,&chunks[i].coord,sizeof(chunks[i].coord))==0) {
                slots[i]=slot;break;
            }
        }
    }
    if(total>UINT32_MAX) {free(slots);return error(err,cap,"Checkpoint reactive address range overflow");}
    uint32_t count=(uint32_t)total;
    identity_t *ids=malloc((size_t)payload->body_count*sizeof(*ids));
    binding_t *bindings=count?malloc((size_t)count*sizeof(*bindings)):NULL;
    fr_reactive_arena_request_t *requests=count?calloc(count,sizeof(*requests)):NULL;
    fr_reactive_handle_t *handles=count?malloc((size_t)count*sizeof(*handles)):NULL;
    uint32_t **pending_handles=archive->chunk_count?calloc(archive->chunk_count,sizeof(*pending_handles)):NULL;
    if(!ids||(archive->chunk_count&&!pending_handles)||(count&&(!bindings||!requests||!handles))) {
        free(slots);free(ids);free(bindings);free(requests);free(handles);free(pending_handles);return error(err,cap,"Cannot stage immutable checkpoint body bindings");
    }
    for(uint32_t i=0;i<payload->body_count;i++) ids[i]=(identity_t){payload->bodies[i].body.id,i};
    qsort(ids,payload->body_count,sizeof(*ids),identity_order);
    bool okay=true,particles=false;
    for(uint32_t i=0;okay&&i<archive->body_count;i++) {
        okay=body_binding(archive->bodies+i,ids,payload->body_count,payload,pixels,pixel_words,bindings+i);
        requests[i]=(fr_reactive_arena_request_t){.operation=FR_REACTIVE_ARENA_ALLOCATE,.record=archive->bodies[i]};
        particles|=archive->bodies[i].state.material==DC_MATERIAL_LEAVES;
    }
    free(ids);
    uint32_t at=archive->body_count;
    for(uint32_t i=0;okay&&i<archive->chunk_count;i++) {
        if(archive->chunks[i].count) pending_handles[i]=malloc((size_t)archive->chunks[i].count*8u);
        if(archive->chunks[i].count&&!pending_handles[i]) {okay=false;break;}
        for(uint32_t j=0;j<archive->chunks[i].count;j++) {
            const fr_reactive_attachment_t *attachment=archive->chunks[i].attachments+j;
            uint32_t kind=attachment->record.owner.kind;
            bindings[at]=(binding_t){slots[i]!=UINT32_MAX?kind:0u,
                slots[i]!=UINT32_MAX?slots[i]*(kind==FR_REACTIVE_OWNER_CELL?4096u:8192u)+attachment->local_index:0u,0};
            requests[at++]=(fr_reactive_arena_request_t){.operation=FR_REACTIVE_ARENA_ALLOCATE,.record=attachment->record};
            particles|=kind==FR_REACTIVE_OWNER_PARTICLE||attachment->record.state.material==DC_MATERIAL_LEAVES;
        }
    }
    free(slots);
    struct fr_reactive_gpu *pending=NULL;
    if(okay) okay=fr_gpu_reactive_storage_create(g,dc_gpu_pixel_pool_capacity(g),&pending,err,cap);
    if(okay&&particles) okay=fr_gpu_reactive_storage_particles(g,pending,err,cap);
    uint32_t required=0;
    if(okay&&count) okay=fr_reactive_arena_apply(g,&pending->arena,requests,count,handles,&required,err,cap);
    if(okay) okay=fr_gpu_reactive_storage_stream(g,pending,err,cap);
    if(okay) {
        for(uint32_t i=0;i<count;i++) {
            if(bindings[i].kind==0u) continue;
            uint32_t *destination=NULL;size_t offset=(size_t)bindings[i].word*2u;
            if(bindings[i].kind==FR_REACTIVE_OWNER_BODY) {destination=pending->shapes.mapped;offset=((size_t)bindings[i].word*8u+bindings[i].nibble)*2u;}
            else if(bindings[i].kind==FR_REACTIVE_OWNER_CELL) destination=pending->cells.mapped;
            else destination=pending->particles.mapped;
            memcpy(destination+offset,handles+i,sizeof(*handles));
        }
        uint32_t first=archive->body_count;
        for(uint32_t i=0;i<archive->chunk_count;i++) {
            if(archive->chunks[i].count) memcpy(pending_handles[i],handles+first,(size_t)archive->chunks[i].count*8u);
            first+=archive->chunks[i].count;
        }
        pending->high_id=archive->next_id-1u;*out=pending;*out_handles=pending_handles;
    } else {
        fr_gpu_reactive_storage_destroy(g,pending);
        for(uint32_t i=0;i<archive->chunk_count;i++) free(pending_handles[i]);
        free(pending_handles);
        error(err,cap,"Invalid/unallocatable checkpoint reaction body bindings");
    }
    free(bindings);free(requests);free(handles);return okay;
}
void fr_gpu_reactive_checkpoint_publish(dc_gpu_t *g,struct fr_reactive_gpu *prepared) {
    struct fr_reactive_gpu *old=g->reactive;g->reactive=prepared;
    fr_gpu_reactive_bind_work(g,g->runtime_bodies->work.mapped);g->descriptor_revision++;
    fr_gpu_reactive_storage_destroy(g,old);
}
