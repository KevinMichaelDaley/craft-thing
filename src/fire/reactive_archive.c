#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/reactive_archive.h"
enum {HEADER_BYTES=40};
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static void put(uint8_t *bytes,uint64_t value,uint32_t count) {
    for(uint32_t i=0;i<count;i++) bytes[i]=(uint8_t)(value>>(i*8u));
}
static uint64_t get(const uint8_t *bytes,uint32_t count) {
    uint64_t value=0;for(uint32_t i=0;i<count;i++) value|=(uint64_t)bytes[i]<<(i*8u);return value;
}
static uint64_t hash(const uint8_t *bytes,size_t length) {
    uint64_t value=UINT64_C(14695981039346656037);
    for(size_t i=0;i<length;i++) if(i<32u||i>=HEADER_BYTES) value=(value^bytes[i])*UINT64_C(1099511628211);
    return value;
}
static bool valid(const fr_reactive_archive_t *archive,const dc_chunk_t *physical,uint32_t count,char *err,uint32_t cap) {
    if(!archive||archive->chunk_count!=count||(!physical&&count)||(!archive->chunks&&count)||
       (!archive->bodies&&archive->body_count)||!archive->next_id)
        return error(err,cap,"Invalid reactive archive channels");
    uint64_t total=archive->body_count;
    for(uint32_t i=0;i<count;i++) {
        if(archive->chunks[i].next_id>archive->next_id||archive->chunks[i].next_id==0u)
            return error(err,cap,"Reactive archive cannot roll back a chunk identity counter");
        fr_reactive_chunk_t channel=archive->chunks[i];channel.next_id=archive->next_id;
        if(!fr_reactive_chunk_valid(&channel,physical+i,err,cap)) return false;
        total+=channel.count;
    }
    if(total>UINT32_MAX) return error(err,cap,"Reactive archive record count exceeds its address space");
    for(uint32_t i=0;i<archive->body_count;i++) if(archive->bodies[i].owner.kind!=FR_REACTIVE_OWNER_BODY)
        return error(err,cap,"Non-body owner in reactive body checkpoint channel");
    size_t bytes=0,written=0;
    if(!fr_reactive_snapshot_size((uint32_t)total,&bytes)) return error(err,cap,"Reactive archive validation size overflow");
    uint8_t *packet=malloc(bytes);
    fr_reactive_record_t *records=total?malloc((size_t)total*sizeof(*records)):NULL;
    if(!packet||(total&&!records)) {free(packet);free(records);return error(err,cap,"Cannot validate immutable reactive archive");}
    uint32_t at=0;
    for(uint32_t i=0;i<archive->body_count;i++) records[at++]=archive->bodies[i];
    for(uint32_t i=0;i<count;i++) for(uint32_t j=0;j<archive->chunks[i].count;j++)
        records[at++]=archive->chunks[i].attachments[j].record;
    bool okay=fr_reactive_snapshot_encode(records,(uint32_t)total,archive->next_id,packet,bytes,&written,err,cap);
    free(packet);free(records);return okay;
}
bool fr_reactive_archive_encode(const fr_reactive_archive_t *archive,const dc_chunk_t *chunks,
    uint32_t chunk_count,uint8_t **bytes,size_t *length,char *err,uint32_t cap) {
    if(!bytes||!length||!valid(archive,chunks,chunk_count,err,cap)) return false;
    size_t body_bytes=0,total=HEADER_BYTES,written=0;
    if(!fr_reactive_snapshot_size(archive->body_count,&body_bytes)||body_bytes>SIZE_MAX-total)
        return error(err,cap,"Reactive archive size overflow");
    total+=body_bytes;
    for(uint32_t i=0;i<chunk_count;i++) {
        size_t size=0;
        if(!fr_reactive_chunk_size(archive->chunks[i].count,&size)||size>SIZE_MAX-8u||total>SIZE_MAX-size-8u)
            return error(err,cap,"Reactive archive chunk size overflow");
        total+=8u+size;
    }
    uint8_t *staged=calloc(1,total);
    if(!staged) return error(err,cap,"Cannot stage reactive checkpoint archive");
    put(staged,0x31575246u,4);put(staged+4,1u,4);put(staged+8,chunk_count,4);
    put(staged+16,body_bytes,8);put(staged+24,total,8);
    bool okay=fr_reactive_snapshot_encode(archive->bodies,archive->body_count,archive->next_id,
        staged+HEADER_BYTES,body_bytes,&written,err,cap);
    size_t at=HEADER_BYTES+body_bytes;
    for(uint32_t i=0;okay&&i<chunk_count;i++) {
        fr_reactive_chunk_t channel=archive->chunks[i];channel.next_id=archive->next_id;
        size_t size=0;okay=fr_reactive_chunk_size(channel.count,&size);
        if(okay) {put(staged+at,size,8);at+=8u;okay=fr_reactive_chunk_encode(&channel,chunks+i,staged+at,size,&written,err,cap);at+=size;}
    }
    if(!okay) {free(staged);return false;}
    put(staged+32,hash(staged,total),8);*bytes=staged;*length=total;return true;
}
bool fr_reactive_archive_decode(const uint8_t *bytes,size_t length,const dc_chunk_t *chunks,
    uint32_t chunk_count,fr_reactive_archive_t *out,char *err,uint32_t cap) {
    if(!bytes||!out||(!chunks&&chunk_count)||length<HEADER_BYTES+40u||get(bytes,4)!=0x31575246u||
       get(bytes+4,4)!=1u||get(bytes+8,4)!=chunk_count||get(bytes+12,4)||get(bytes+24,8)!=length||
       hash(bytes,length)!=get(bytes+32,8)) return error(err,cap,"Unsupported/corrupt reactive archive");
    uint64_t body_bytes=get(bytes+16,8);size_t expected=0;
    if(body_bytes<40u||body_bytes>length-HEADER_BYTES) return error(err,cap,"Reactive body packet length mismatch");
    uint32_t body_count=(uint32_t)get(bytes+HEADER_BYTES+16u,4);
    if(!fr_reactive_snapshot_size(body_count,&expected)||expected!=body_bytes) return error(err,cap,"Reactive body record count mismatch");
    fr_reactive_archive_t staged={.body_count=body_count,.chunk_count=chunk_count};
    staged.bodies=body_count?malloc((size_t)body_count*sizeof(*staged.bodies)):NULL;
    staged.chunks=chunk_count?calloc(chunk_count,sizeof(*staged.chunks)):NULL;
    if((body_count&&!staged.bodies)||(chunk_count&&!staged.chunks)) {
        fr_reactive_archive_release(&staged);return error(err,cap,"Cannot stage reactive archive restore");
    }
    uint32_t decoded=0,required=0;
    bool okay=fr_reactive_snapshot_decode(bytes+HEADER_BYTES,(size_t)body_bytes,staged.bodies,body_count,
        &decoded,&staged.next_id,&required,err,cap)&&decoded==body_count;
    size_t at=HEADER_BYTES+(size_t)body_bytes;
    for(uint32_t i=0;okay&&i<chunk_count;i++) {
        if(at>length||length-at<8u) {okay=false;break;}
        uint64_t size=get(bytes+at,8);at+=8u;
        if(size>length-at) {okay=false;break;}
        okay=fr_reactive_chunk_decode(bytes+at,(size_t)size,chunks+i,staged.chunks+i,err,cap)&&staged.chunks[i].next_id==staged.next_id;
        at+=(size_t)size;
    }
    okay=okay&&at==length&&valid(&staged,chunks,chunk_count,err,cap);
    if(!okay) {fr_reactive_archive_release(&staged);return error(err,cap,"Reactive archive channels failed validation");}
    *out=staged;return true;
}
void fr_reactive_archive_release(fr_reactive_archive_t *archive) {
    if(!archive) return;
    for(uint32_t i=0;archive->chunks&&i<archive->chunk_count;i++) fr_reactive_chunk_release(archive->chunks+i);
    free(archive->chunks);free(archive->bodies);*archive=(fr_reactive_archive_t){0};
}
