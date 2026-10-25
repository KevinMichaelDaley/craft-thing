#include <stdlib.h>
#include <string.h>
#include "reactive_storage_gpu.h"
#include "structural_store_channels.h"

static void put64(uint8_t *p,uint64_t value) {for(uint32_t i=0;i<8;i++) p[i]=(uint8_t)(value>>(8*i));}
static uint64_t get64(const uint8_t *p) {uint64_t value=0;for(uint32_t i=0;i<8;i++) value|=(uint64_t)p[i]<<(8*i);return value;}
static uint64_t hash_bytes(uint64_t hash,const void *source,size_t count) {
    const uint8_t *bytes=source;
    for(size_t i=0;i<count;i++) hash=(hash^bytes[i])*UINT64_C(1099511628211);
    return hash;
}
void dc_checkpoint_channels_discard(dc_gpu_t *g,dc_checkpoint_channels_t *p) {
    if(!p) return;
    fr_thermal_checkpoint_discard(g,p->prepared_thermal);
    fr_gpu_reactive_storage_destroy(g,p->prepared_reactive);
    if(p->prepared_handles) {
        for(uint32_t i=0;i<p->reactive.chunk_count;i++) free(p->prepared_handles[i]);
        free(p->prepared_handles);
    }
    fr_reactive_archive_release(&p->reactive);fr_thermal_checkpoint_release(&p->thermal);
    free(p->reactive_bytes);free(p->thermal_bytes);memset(p,0,sizeof(*p));
}
bool dc_checkpoint_channels_collect(dc_gpu_t *g,const dc_chunk_t *chunks,uint32_t count,
    dc_checkpoint_channels_t *out,char *err,uint32_t cap) {
    dc_checkpoint_channels_t value={0};bool okay=true;
    if(g->reactive) okay=fr_gpu_reactive_checkpoint_collect(g,chunks,count,&value.reactive,err,cap)&&
        fr_reactive_archive_encode(&value.reactive,chunks,count,&value.reactive_bytes,&value.reactive_length,err,cap);
    if(okay&&g->thermal) okay=fr_thermal_checkpoint_collect(g,chunks,count,&value.thermal,err,cap)&&
        fr_thermal_checkpoint_encode(&value.thermal,chunks,count,&value.thermal_bytes,&value.thermal_length,err,cap);
    if(!okay) {dc_checkpoint_channels_discard(g,&value);return false;}
    *out=value;return true;
}
uint64_t dc_checkpoint_channels_hash(uint64_t hash,const dc_checkpoint_channels_t *p) {
    uint8_t lengths[16];put64(lengths,p->reactive_length);put64(lengths+8,p->thermal_length);
    hash=hash_bytes(hash,lengths,sizeof(lengths));
    hash=hash_bytes(hash,p->reactive_bytes,p->reactive_length);
    return hash_bytes(hash,p->thermal_bytes,p->thermal_length);
}
bool dc_checkpoint_channels_write(FILE *file,const dc_checkpoint_channels_t *p) {
    uint8_t lengths[16];put64(lengths,p->reactive_length);put64(lengths+8,p->thermal_length);
    return fwrite(lengths,1,sizeof(lengths),file)==sizeof(lengths)&&
        (!p->reactive_length||fwrite(p->reactive_bytes,1,p->reactive_length,file)==p->reactive_length)&&
        (!p->thermal_length||fwrite(p->thermal_bytes,1,p->thermal_length,file)==p->thermal_length);
}
bool dc_checkpoint_channels_read(FILE *file,dc_checkpoint_channels_t *out) {
    uint8_t lengths[16];if(fread(lengths,1,sizeof(lengths),file)!=sizeof(lengths)) return false;
    uint64_t a=get64(lengths),b=get64(lengths+8);
    if(a>SIZE_MAX||b>SIZE_MAX||a>UINT64_MAX-b||(b&&!a)) return false;
    long start=ftell(file);if(start<0||fseek(file,0,SEEK_END)!=0) return false;
    long end=ftell(file);if(end<start||fseek(file,start,SEEK_SET)!=0||(uint64_t)(end-start)!=a+b) return false;
    dc_checkpoint_channels_t value={.reactive_length=(size_t)a,.thermal_length=(size_t)b};
    value.reactive_bytes=a?malloc((size_t)a):NULL;value.thermal_bytes=b?malloc((size_t)b):NULL;
    bool okay=(!a||(value.reactive_bytes&&fread(value.reactive_bytes,1,(size_t)a,file)==a))&&
        (!b||(value.thermal_bytes&&fread(value.thermal_bytes,1,(size_t)b,file)==b));
    if(!okay) {free(value.reactive_bytes);free(value.thermal_bytes);return false;}
    *out=value;return true;
}
bool dc_checkpoint_channels_prepare(dc_gpu_t *g,dc_checkpoint_channels_t *p,
    const dc_chunk_t *chunks,uint32_t count,const dc_structural_store_payload_t *payload,
    const uint32_t *pixels,uint32_t pixel_words,char *err,uint32_t cap) {
    bool okay=!p->reactive_length||fr_reactive_archive_decode(p->reactive_bytes,p->reactive_length,
        chunks,count,&p->reactive,err,cap);
    if(okay&&p->thermal_length) okay=p->reactive_length&&fr_thermal_checkpoint_decode(p->thermal_bytes,p->thermal_length,
        chunks,count,&p->thermal,err,cap);
    if(okay) okay=fr_gpu_reactive_checkpoint_prepare(g,p->reactive_length?&p->reactive:NULL,
        chunks,payload,pixels,pixel_words,&p->prepared_reactive,&p->prepared_handles,err,cap);
    if(okay) okay=fr_thermal_checkpoint_prepare(g,p->prepared_reactive,p->thermal_length?&p->thermal:NULL,
        chunks,&p->prepared_thermal,err,cap);
    return okay;
}
void dc_checkpoint_channels_publish(dc_gpu_t *g,dc_checkpoint_channels_t *p,
    dc_chunk_t *chunks,dc_chunk_colors_t *colors,uint32_t count) {
    fr_gpu_reactive_checkpoint_clear_pending(g);
    free(g->checkpoint_chunks);free(g->checkpoint_colors);
    g->checkpoint_chunks=chunks;g->checkpoint_colors=colors;g->checkpoint_chunk_count=count;
    fr_gpu_reactive_checkpoint_publish(g,p->prepared_reactive);p->prepared_reactive=NULL;
    fr_thermal_checkpoint_publish(g,p->prepared_thermal);p->prepared_thermal=NULL;
    g->checkpoint_reactive=p->reactive.chunks;p->reactive.chunks=NULL;p->reactive.chunk_count=0;
    g->checkpoint_reactive_handles=p->prepared_handles;p->prepared_handles=NULL;
}
