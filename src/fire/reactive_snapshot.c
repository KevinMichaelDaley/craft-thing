#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ferrum/fire/reactive_state.h"

enum { HEADER_WORDS=10, RECORD_WORDS=FR_REACTIVE_STATE_WORDS+FR_REACTIVE_OWNER_WORDS };
_Static_assert(sizeof(fr_reactive_record_t)==RECORD_WORDS*4u,"Reactive snapshot record layout changed");
static bool error(char *err,uint32_t cap,const char *reason) {
    if(err&&cap) snprintf(err,cap,"%s",reason);
    return false;
}
static uint64_t parcel_id(const fr_reactive_record_t *r) {
    return (uint64_t)r->state.parcel_id_low|((uint64_t)r->state.parcel_id_high<<32);
}
static int owner_order(const void *a,const void *b) {
    const fr_reactive_record_t *first=a,*second=b;
    uint32_t x[FR_REACTIVE_OWNER_WORDS],y[FR_REACTIVE_OWNER_WORDS];
    memcpy(x,&first->owner,sizeof(x));memcpy(y,&second->owner,sizeof(y));
    for(unsigned i=0;i<FR_REACTIVE_OWNER_WORDS;i++) {
        if(x[i]<y[i]) return -1;
        if(x[i]>y[i]) return 1;
    }
    return 0;
}
static int parcel_order(const void *a,const void *b) {
    uint64_t x=parcel_id(a),y=parcel_id(b);
    return x<y?-1:x>y?1:0;
}
static void encode(uint8_t *target,const uint32_t *words,size_t count) {
    for(size_t i=0;i<count;i++) for(unsigned byte=0;byte<4;byte++)
        target[i*4+byte]=(uint8_t)(words[i]>>(byte*8));
}
static void decode(uint32_t *target,const uint8_t *bytes,size_t count) {
    for(size_t i=0;i<count;i++) target[i]=(uint32_t)bytes[i*4]|
        ((uint32_t)bytes[i*4+1]<<8)|((uint32_t)bytes[i*4+2]<<16)|((uint32_t)bytes[i*4+3]<<24);
}
static uint64_t hash_bytes(uint64_t hash,const uint8_t *bytes,size_t count) {
    for(size_t i=0;i<count;i++) hash=(hash^bytes[i])*UINT64_C(1099511628211);
    return hash;
}
static bool records_valid(fr_reactive_record_t *records,uint32_t count,uint64_t next,
                           char *err,uint32_t cap) {
    if(next==0) return error(err,cap,"Reactive identity counter cannot wrap");
    for(uint32_t i=0;i<count;i++)
        if(!fr_reactive_owner_valid(&records[i].owner,err,cap)||
           !fr_reactive_state_valid(&records[i].state,err,cap)||parcel_id(&records[i])>=next||
           ((uint64_t)records[i].state.origin_id_low|((uint64_t)records[i].state.origin_id_high<<32))>=next)
            return error(err,cap,"Invalid reactive snapshot identity/state/counter");
    if(count>1) qsort(records,count,sizeof(*records),parcel_order);
    for(uint32_t i=1;i<count;i++) if(parcel_id(&records[i-1])==parcel_id(&records[i]))
        return error(err,cap,"Duplicate reactive parcel identity");
    if(count>1) qsort(records,count,sizeof(*records),owner_order);
    for(uint32_t i=1;i<count;i++) if(owner_order(&records[i-1],&records[i])==0)
        return error(err,cap,"Duplicate reactive owner");
    return true;
}
static bool legacy_owners_valid(const fr_reactive_record_t *records,uint32_t count,
                                uint32_t version,char *err,uint32_t cap) {
    for(uint32_t i=0;version==1u&&i<count;i++)
        if(records[i].owner.kind==FR_REACTIVE_OWNER_PARTICLE&&records[i].owner.key[3]!=0u)
            return error(err,cap,"Reserved ABI 1 particle owner bits are nonzero");
    return true;
}

bool fr_reactive_snapshot_write(const char *path,const fr_reactive_record_t *records,
                                 uint32_t count,uint64_t next,char *err,uint32_t cap) {
    if(!path||!*path||(!records&&count))
        return error(err,cap,"Invalid reactive snapshot write request");
    size_t bytes=(size_t)count*sizeof(*records);
    if(count&&bytes/count!=sizeof(*records))
        return error(err,cap,"Reactive snapshot size overflow");
    fr_reactive_record_t *copy=bytes?malloc(bytes):NULL;
    uint8_t *payload=bytes?malloc(bytes):NULL;
    if(bytes&&(!copy||!payload)) {free(copy);free(payload);return error(err,cap,"Cannot stage reactive snapshot");}
    if(bytes) memcpy(copy,records,bytes);
    if(!records_valid(copy,count,next,err,cap)) {free(copy);free(payload);return false;}
    for(uint32_t i=0;i<count;i++) {
        uint32_t words[RECORD_WORDS];memcpy(words,copy+i,sizeof(words));
        encode(payload+(size_t)i*sizeof(*copy),words,RECORD_WORDS);
    }
    free(copy);
    uint32_t header[HEADER_WORDS]={0x31535246u,FR_REACTIVE_STATE_ABI,FR_REACTIVE_STATE_WORDS,
        FR_REACTIVE_OWNER_WORDS,count,0,(uint32_t)next,(uint32_t)(next>>32),0,0};
    uint8_t encoded[HEADER_WORDS*4];encode(encoded,header,HEADER_WORDS);
    uint64_t hash=hash_bytes(UINT64_C(14695981039346656037),encoded,8u*4u);
    hash=hash_bytes(hash,payload,bytes);header[8]=(uint32_t)hash;header[9]=(uint32_t)(hash>>32);
    encode(encoded,header,HEADER_WORDS);
    char temporary[4096];int length=snprintf(temporary,sizeof(temporary),"%s.tmp",path);
    if(length<0||(size_t)length>=sizeof(temporary)) {free(payload);return error(err,cap,"Reactive snapshot path too long");}
    FILE *file=fopen(temporary,"wb");
    if(!file) {free(payload);return error(err,cap,"Cannot create reactive snapshot transaction");}
    bool okay=fwrite(encoded,sizeof(encoded),1,file)==1;
    if(okay&&bytes) okay=fwrite(payload,bytes,1,file)==1;
    if(fclose(file)!=0) okay=false;
    free(payload);
    if(okay) okay=rename(temporary,path)==0;
    if(!okay) {remove(temporary);return error(err,cap,"Cannot publish reactive snapshot");}
    return true;
}
bool fr_reactive_snapshot_read(const char *path,fr_reactive_record_t *records,uint32_t capacity,
                                uint32_t *count,uint64_t *next,uint32_t *required,char *err,uint32_t cap) {
    if(!path||!*path||!count||!next||!required||(!records&&capacity))
        return error(err,cap,"Invalid reactive snapshot read request");
    FILE *file=fopen(path,"rb");
    if(!file) {
        if(errno==ENOENT) {*count=0;*next=1;*required=0;return true;}
        return error(err,cap,"Cannot open reactive snapshot");
    }
    uint8_t encoded[HEADER_WORDS*4];uint32_t header[HEADER_WORDS];
    bool okay=fread(encoded,sizeof(encoded),1,file)==1;
    if(okay) decode(header,encoded,HEADER_WORDS);
    if(!okay||header[0]!=0x31535246u||(header[1]!=1u&&header[1]!=FR_REACTIVE_STATE_ABI)||
       header[2]!=FR_REACTIVE_STATE_WORDS||header[3]!=FR_REACTIVE_OWNER_WORDS||header[5]!=0) {
        fclose(file);return error(err,cap,"Unsupported/corrupt reactive snapshot header");
    }
    uint64_t bytes64=(uint64_t)header[4]*sizeof(fr_reactive_record_t);
    if(bytes64>SIZE_MAX||bytes64>LONG_MAX-sizeof(encoded)||fseek(file,0,SEEK_END)!=0) {
        fclose(file);return error(err,cap,"Reactive snapshot size is not representable");
    }
    long actual=ftell(file);
    if(actual<0||(uint64_t)actual!=bytes64+sizeof(encoded)||fseek(file,sizeof(encoded),SEEK_SET)!=0) {
        fclose(file);return error(err,cap,"Reactive snapshot length mismatch");
    }
    if(header[4]>capacity) {
        *required=header[4];fclose(file);return error(err,cap,"Reactive snapshot requires more record capacity");
    }
    size_t bytes=(size_t)bytes64;
    uint8_t *payload=bytes?malloc(bytes):NULL;
    fr_reactive_record_t *copy=bytes?malloc(bytes):NULL;
    if(bytes&&(!payload||!copy)) {
        free(payload);free(copy);fclose(file);return error(err,cap,"Cannot stage reactive restore");
    }
    if(bytes) okay=fread(payload,bytes,1,file)==1;
    if(fclose(file)!=0) okay=false;
    if(!okay) {free(payload);free(copy);return error(err,cap,"Truncated reactive snapshot payload");}
    uint64_t hash=hash_bytes(UINT64_C(14695981039346656037),encoded,8u*4u);
    hash=hash_bytes(hash,payload,bytes);
    uint64_t stored=(uint64_t)header[8]|((uint64_t)header[9]<<32);
    if(hash!=stored) {free(payload);free(copy);return error(err,cap,"Reactive snapshot checksum mismatch");}
    for(uint32_t i=0;i<header[4];i++) {
        uint32_t words[RECORD_WORDS];decode(words,payload+(size_t)i*sizeof(*copy),RECORD_WORDS);
        memcpy(copy+i,words,sizeof(words));
    }
    free(payload);
    uint64_t restored_next=(uint64_t)header[6]|((uint64_t)header[7]<<32);
    if(!legacy_owners_valid(copy,header[4],header[1],err,cap)||
       !records_valid(copy,header[4],restored_next,err,cap)) {free(copy);return false;}
    if(bytes) memcpy(records,copy,bytes);
    free(copy);*count=header[4];*next=restored_next;*required=header[4];return true;
}
bool fr_reactive_snapshot_size(uint32_t count,size_t *out) {
    if(!out) return false;
    size_t bytes=(size_t)count*sizeof(fr_reactive_record_t);
    if((count&&bytes/count!=sizeof(fr_reactive_record_t))||bytes>SIZE_MAX-HEADER_WORDS*4u) return false;
    *out=bytes+HEADER_WORDS*4u;return true;
}
bool fr_reactive_snapshot_encode(const fr_reactive_record_t *records,uint32_t count,uint64_t next,
    uint8_t *bytes,size_t capacity,size_t *written,char *err,uint32_t cap) {
    size_t total=0;
    if(!bytes||!written||(!records&&count)||!fr_reactive_snapshot_size(count,&total)||capacity<total)
        return error(err,cap,"Invalid or insufficient reactive packet destination");
    size_t payload=total-HEADER_WORDS*4u;
    fr_reactive_record_t *copy=payload?malloc(payload):NULL;
    if(payload&&!copy) return error(err,cap,"Cannot stage reactive packet");
    if(payload) memcpy(copy,records,payload);
    if(!records_valid(copy,count,next,err,cap)) {free(copy);return false;}
    uint32_t header[HEADER_WORDS]={0x31535246u,FR_REACTIVE_STATE_ABI,FR_REACTIVE_STATE_WORDS,
        FR_REACTIVE_OWNER_WORDS,count,0,(uint32_t)next,(uint32_t)(next>>32),0,0};
    encode(bytes,header,HEADER_WORDS);
    for(uint32_t i=0;i<count;i++) {
        uint32_t words[RECORD_WORDS];memcpy(words,copy+i,sizeof(words));
        encode(bytes+HEADER_WORDS*4u+(size_t)i*sizeof(*copy),words,RECORD_WORDS);
    }
    free(copy);
    uint64_t hash=hash_bytes(UINT64_C(14695981039346656037),bytes,8u*4u);
    hash=hash_bytes(hash,bytes+HEADER_WORDS*4u,payload);
    header[8]=(uint32_t)hash;header[9]=(uint32_t)(hash>>32);encode(bytes,header,HEADER_WORDS);
    *written=total;return true;
}
bool fr_reactive_snapshot_decode(const uint8_t *bytes,size_t length,fr_reactive_record_t *records,
    uint32_t capacity,uint32_t *count,uint64_t *next,uint32_t *required,char *err,uint32_t cap) {
    if(!bytes||length<HEADER_WORDS*4u||!count||!next||!required||(!records&&capacity))
        return error(err,cap,"Invalid reactive packet restore request");
    uint32_t header[HEADER_WORDS];decode(header,bytes,HEADER_WORDS);size_t total=0;
    if(header[0]!=0x31535246u||(header[1]!=1u&&header[1]!=FR_REACTIVE_STATE_ABI)||header[2]!=FR_REACTIVE_STATE_WORDS||
       header[3]!=FR_REACTIVE_OWNER_WORDS||header[5]!=0||!fr_reactive_snapshot_size(header[4],&total))
        return error(err,cap,"Unsupported/corrupt reactive packet header");
    if(length!=total) return error(err,cap,"Reactive packet length mismatch");
    size_t payload=total-HEADER_WORDS*4u;
    uint64_t hash=hash_bytes(UINT64_C(14695981039346656037),bytes,8u*4u);
    hash=hash_bytes(hash,bytes+HEADER_WORDS*4u,payload);
    if(hash!=((uint64_t)header[8]|((uint64_t)header[9]<<32))) return error(err,cap,"Reactive packet checksum mismatch");
    if(header[4]>capacity) {*required=header[4];return error(err,cap,"Reactive packet requires more record capacity");}
    fr_reactive_record_t *copy=payload?malloc(payload):NULL;
    if(payload&&!copy) return error(err,cap,"Cannot stage reactive packet restore");
    for(uint32_t i=0;i<header[4];i++) {
        uint32_t words[RECORD_WORDS];decode(words,bytes+HEADER_WORDS*4u+(size_t)i*sizeof(*copy),RECORD_WORDS);
        memcpy(copy+i,words,sizeof(words));
    }
    uint64_t restored_next=(uint64_t)header[6]|((uint64_t)header[7]<<32);
    if(!legacy_owners_valid(copy,header[4],header[1],err,cap)||
       !records_valid(copy,header[4],restored_next,err,cap)) {free(copy);return false;}
    if(payload) memcpy(records,copy,payload);
    free(copy);*count=header[4];*next=restored_next;*required=header[4];return true;
}
