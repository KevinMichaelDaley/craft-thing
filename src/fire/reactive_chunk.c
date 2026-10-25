#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/reactive_chunk.h"

enum {HEADER_BYTES=32,PHYSICAL_ROWS=DC_CHUNK_CELLS+DC_MPM_PARTICLES_PER_CHUNK};
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static void put(uint8_t *bytes,uint32_t value) {
    for(uint32_t i=0;i<4;i++) bytes[i]=(uint8_t)(value>>(i*8u));
}
static uint32_t get(const uint8_t *bytes) {
    return (uint32_t)bytes[0]|((uint32_t)bytes[1]<<8)|((uint32_t)bytes[2]<<16)|((uint32_t)bytes[3]<<24);
}
static uint64_t hash(const uint8_t *bytes,size_t length) {
    uint64_t value=UINT64_C(14695981039346656037);
    for(size_t i=0;i<length;i++) if(i<16||i>=24) value=(value^bytes[i])*UINT64_C(1099511628211);
    return value;
}
static int order(const void *a,const void *b) {
    const fr_reactive_attachment_t *x=a,*y=b;
    uint32_t first[FR_REACTIVE_OWNER_WORDS],second[FR_REACTIVE_OWNER_WORDS];
    memcpy(first,&x->record.owner,sizeof(first));memcpy(second,&y->record.owner,sizeof(second));
    for(uint32_t i=0;i<FR_REACTIVE_OWNER_WORDS;i++) {
        if(first[i]<second[i]) return -1;
        if(first[i]>second[i]) return 1;
    }
    return 0;
}
bool fr_reactive_chunk_size(uint32_t count,size_t *out) {
    size_t state=0;
    if(!out||count>PHYSICAL_ROWS||!fr_reactive_snapshot_size(count,&state)||
       state>SIZE_MAX-HEADER_BYTES-(size_t)count*4u) return false;
    *out=HEADER_BYTES+(size_t)count*4u+state;return true;
}
static bool bindings_valid(const fr_reactive_chunk_t *channel,const dc_chunk_t *chunk,char *err,uint32_t cap) {
    if(!channel||!chunk||channel->count>PHYSICAL_ROWS||(!channel->attachments&&channel->count))
        return error(err,cap,"Invalid reactive chunk channel");
    uint8_t occupied[PHYSICAL_ROWS]={0};
    for(uint32_t i=0;i<channel->count;i++) {
        const fr_reactive_attachment_t *a=channel->attachments+i;
        const fr_reactive_owner_t *owner=&a->record.owner;
        uint32_t index=a->local_index,row=index;
        if(owner->kind==FR_REACTIVE_OWNER_CELL) {
            fr_reactive_owner_t expected=fr_reactive_cell_owner(chunk->coord,index);
            if(index>=DC_CHUNK_CELLS||memcmp(owner,&expected,sizeof(expected))||
               chunk->cells[index].material!=a->record.state.material||chunk->cells[index].material==DC_MATERIAL_AIR)
                return error(err,cap,"Reactive cell binding differs from physical chunk");
        } else if(owner->kind==FR_REACTIVE_OWNER_PARTICLE) {
            if(index>=DC_MPM_PARTICLES_PER_CHUNK) return error(err,cap,"Reactive particle row is outside chunk");
            const dc_mpm_particle_t *p=chunk->particles+index;
            if(!p->mass_fp||p->material!=a->record.state.material||p->id_lo!=owner->key[0]||p->id_hi!=owner->key[1])
                return error(err,cap,"Reactive particle binding differs from physical chunk");
            if(index<DC_CHUNK_CELLS&&(chunk->cells[index].flags&DC_CELL_LOADED_TERRAIN)&&
               chunk->cells[index].material==p->material)
                return error(err,cap,"Held terrain mirror cannot also own particle reaction state");
            row+=DC_CHUNK_CELLS;
        } else return error(err,cap,"Chunk channel cannot own body reaction state");
        if(occupied[row]) return error(err,cap,"Duplicate reactive physical binding");
        occupied[row]=1;
    }
    return true;
}
bool fr_reactive_chunk_valid(const fr_reactive_chunk_t *channel,const dc_chunk_t *chunk,char *err,uint32_t cap) {
    if(!bindings_valid(channel,chunk,err,cap)) return false;
    size_t size=0,written=0;
    if(!fr_reactive_snapshot_size(channel->count,&size)) return error(err,cap,"Reactive chunk packet size overflow");
    fr_reactive_record_t *records=channel->count?malloc((size_t)channel->count*sizeof(*records)):NULL;
    uint8_t *bytes=malloc(size);
    if(!bytes||(channel->count&&!records)) {free(bytes);free(records);return error(err,cap,"Cannot validate reactive chunk packet");}
    for(uint32_t i=0;i<channel->count;i++) records[i]=channel->attachments[i].record;
    bool okay=fr_reactive_snapshot_encode(records,channel->count,channel->next_id,bytes,size,&written,err,cap);
    free(bytes);free(records);return okay;
}
bool fr_reactive_chunk_encode(const fr_reactive_chunk_t *channel,const dc_chunk_t *chunk,
    uint8_t *bytes,size_t capacity,size_t *written,char *err,uint32_t cap) {
    size_t size=0;
    if(!bytes||!written||!channel||!fr_reactive_chunk_size(channel->count,&size)||capacity<size||
       !bindings_valid(channel,chunk,err,cap)) return error(err,cap,"Invalid reactive chunk encode request");
    uint8_t *staged=calloc(1,size);
    fr_reactive_attachment_t *sorted=channel->count?malloc((size_t)channel->count*sizeof(*sorted)):NULL;
    fr_reactive_record_t *records=channel->count?malloc((size_t)channel->count*sizeof(*records)):NULL;
    if(!staged||(channel->count&&(!sorted||!records))) {
        free(staged);free(sorted);free(records);return error(err,cap,"Cannot stage reactive chunk encode");
    }
    if(channel->count) memcpy(sorted,channel->attachments,(size_t)channel->count*sizeof(*sorted));
    if(channel->count>1) qsort(sorted,channel->count,sizeof(*sorted),order);
    for(uint32_t i=0;i<channel->count;i++) {records[i]=sorted[i].record;put(staged+HEADER_BYTES+(size_t)i*4u,sorted[i].local_index);}
    size_t start=HEADER_BYTES+(size_t)channel->count*4u,encoded=0;
    bool okay=fr_reactive_snapshot_encode(records,channel->count,channel->next_id,staged+start,size-start,&encoded,err,cap);
    free(sorted);free(records);
    if(!okay) {free(staged);return false;}
    put(staged,0x31435246u);put(staged+4,1u);put(staged+8,channel->count);put(staged+12,(uint32_t)encoded);
    uint64_t checksum=hash(staged,size);put(staged+16,(uint32_t)checksum);put(staged+20,(uint32_t)(checksum>>32));
    memcpy(bytes,staged,size);free(staged);*written=size;return true;
}
bool fr_reactive_chunk_decode(const uint8_t *bytes,size_t length,const dc_chunk_t *chunk,
    fr_reactive_chunk_t *out,char *err,uint32_t cap) {
    size_t size=0;
    if(!bytes||!out||length<HEADER_BYTES||get(bytes)!=0x31435246u||get(bytes+4)!=1u||
       get(bytes+24)||get(bytes+28)||!fr_reactive_chunk_size(get(bytes+8),&size)||length!=size)
        return error(err,cap,"Unsupported/corrupt reactive chunk header");
    uint32_t count=get(bytes+8);size_t start=HEADER_BYTES+(size_t)count*4u;
    if(get(bytes+12)!=length-start||hash(bytes,length)!=((uint64_t)get(bytes+16)|((uint64_t)get(bytes+20)<<32)))
        return error(err,cap,"Reactive chunk length/checksum mismatch");
    fr_reactive_chunk_t staged={.count=count};uint32_t decoded=0,required=0;
    fr_reactive_record_t *records=count?malloc((size_t)count*sizeof(*records)):NULL;
    staged.attachments=count?calloc(count,sizeof(*staged.attachments)):NULL;
    if(count&&(!records||!staged.attachments)) {free(records);fr_reactive_chunk_release(&staged);return error(err,cap,"Cannot stage reactive chunk restore");}
    bool okay=fr_reactive_snapshot_decode(bytes+start,length-start,records,count,&decoded,&staged.next_id,&required,err,cap)&&decoded==count;
    for(uint32_t i=0;okay&&i<count;i++) {staged.attachments[i].local_index=get(bytes+HEADER_BYTES+(size_t)i*4u);staged.attachments[i].record=records[i];}
    free(records);
    if(!okay||!bindings_valid(&staged,chunk,err,cap)) {fr_reactive_chunk_release(&staged);return false;}
    *out=staged;return true;
}
void fr_reactive_chunk_release(fr_reactive_chunk_t *channel) {
    if(channel) {free(channel->attachments);*channel=(fr_reactive_chunk_t){0};}
}
