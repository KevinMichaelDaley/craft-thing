#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ferrum/fire/material_profile.h"
#include "ferrum/fire/thermal_checkpoint.h"

enum {THERMAL_HEADER_BYTES=48,THERMAL_RECORD_BYTES=16+DC_CHUNK_CELLS*4};
_Static_assert(sizeof(float)==4&&FLT_RADIX==2&&FLT_MANT_DIG==24,"Thermal checkpoint needs binary32");
static bool error(char *err,uint32_t cap,const char *message) {
    if(err&&cap) snprintf(err,cap,"%s",message);
    return false;
}
static void put32(uint8_t *p,uint32_t value) {for(uint32_t i=0;i<4;i++) p[i]=(uint8_t)(value>>(8*i));}
static void put64(uint8_t *p,uint64_t value) {for(uint32_t i=0;i<8;i++) p[i]=(uint8_t)(value>>(8*i));}
static uint32_t get32(const uint8_t *p) {uint32_t value=0;for(uint32_t i=0;i<4;i++) value|=(uint32_t)p[i]<<(8*i);return value;}
static uint64_t get64(const uint8_t *p) {uint64_t value=0;for(uint32_t i=0;i<8;i++) value|=(uint64_t)p[i]<<(8*i);return value;}
static uint32_t bits(float value) {uint32_t out;memcpy(&out,&value,4);return out;}
static float number(uint32_t value) {float out;memcpy(&out,&value,4);return out;}
static uint64_t checksum(const uint8_t *bytes,size_t length) {
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<length;i++) hash=(hash^((i>=40&&i<48)?0:bytes[i]))*UINT64_C(1099511628211);
    return hash;
}
static int coord_order(const void *a,const void *b) {
    const dc_chunk_coord_t *x=a,*y=b;
    return x->x<y->x?-1:x->x>y->x?1:x->y<y->y?-1:x->y>y->y?1:0;
}
static bool unique_chunks(const dc_chunk_t *chunks,uint32_t count) {
    if(count<2) return true;
    if((uint64_t)count*sizeof(dc_chunk_coord_t)>SIZE_MAX) return false;
    dc_chunk_coord_t *coords=malloc((size_t)count*sizeof(*coords));if(!coords) return false;
    for(uint32_t i=0;i<count;i++) coords[i]=chunks[i].coord;
    qsort(coords,count,sizeof(*coords),coord_order);bool okay=true;
    for(uint32_t i=1;i<count;i++) if(coord_order(coords+i-1,coords+i)==0) {okay=false;break;}
    free(coords);return okay;
}
static bool valid(const fr_thermal_checkpoint_t *state,const dc_chunk_t *chunks,uint32_t count,char *err,uint32_t cap) {
    if(!state||state->chunk_count!=count||state->purpose>FR_THERMAL_WORLD_HEAT_BENCH||
       !isfinite(state->source.excess_energy)||state->source.excess_energy<0||
       state->source.capacity!=(float)FR_THERMAL_PROBE_CAPACITY||
       (count&&(!chunks||!state->water_excess))||!unique_chunks(chunks,count))
        return error(err,cap,"Invalid thermal checkpoint inventories or bindings");
    double specific=fr_reactive_profile(DC_MATERIAL_WATER)->heat_capacity*FR_THERMAL_AMBIENT_K/65536.;
    for(uint32_t c=0;c<count;c++) for(uint32_t i=0;i<DC_CHUNK_CELLS;i++) {
        float excess=state->water_excess[(size_t)c*DC_CHUNK_CELLS+i];uint32_t mass=chunks[c].cells[i].fluid_mass;
        if(!isfinite(excess)||(!mass&&excess!=0.f)||mass*specific+excess<0.) {
            if(err&&cap) snprintf(err,cap,"Invalid thermal liquid at chunk(%lld,%lld) cell%u: mass%u excess%.9g",
                (long long)chunks[c].coord.x,(long long)chunks[c].coord.y,i,mass,(double)excess);
            return false;
        }
    }
    return true;
}
static bool encoded_size(uint32_t count,size_t *out) {
    uint64_t length=THERMAL_HEADER_BYTES+(uint64_t)count*THERMAL_RECORD_BYTES;
    if(length>SIZE_MAX) return false;
    *out=(size_t)length;return true;
}
bool fr_thermal_checkpoint_encode(const fr_thermal_checkpoint_t *state,const dc_chunk_t *chunks,
    uint32_t count,uint8_t **bytes,size_t *length,char *err,uint32_t cap) {
    size_t size=0;
    if(!bytes||!length||!encoded_size(count,&size))
        return error(err,cap,"Invalid thermal checkpoint inventories or bindings");
    if(!valid(state,chunks,count,err,cap)) return false;
    uint8_t *output=calloc(1,size);if(!output) return error(err,cap,"Cannot allocate thermal checkpoint bytes");
    memcpy(output,"THR1",4);put32(output+4,1);put32(output+8,THERMAL_HEADER_BYTES);
    put32(output+12,state->purpose);put32(output+16,count);put32(output+20,bits(FR_THERMAL_AMBIENT_K));
    put32(output+24,bits(fr_reactive_profile(DC_MATERIAL_WATER)->heat_capacity));put32(output+28,FR_PROFILE_ABI);
    put32(output+32,bits(state->source.excess_energy));put32(output+36,bits(state->source.capacity));
    for(uint32_t c=0;c<count;c++) {
        uint8_t *record=output+THERMAL_HEADER_BYTES+(size_t)c*THERMAL_RECORD_BYTES;
        put64(record,(uint64_t)chunks[c].coord.x);put64(record+8,(uint64_t)chunks[c].coord.y);
        for(uint32_t i=0;i<DC_CHUNK_CELLS;i++) put32(record+16+i*4u,bits(state->water_excess[(size_t)c*DC_CHUNK_CELLS+i]));
    }
    put64(output+40,checksum(output,size));*bytes=output;*length=size;return true;
}
bool fr_thermal_checkpoint_decode(const uint8_t *bytes,size_t length,const dc_chunk_t *chunks,
    uint32_t count,fr_thermal_checkpoint_t *out,char *err,uint32_t cap) {
    size_t expected=0;
    if(!bytes||!out||!encoded_size(count,&expected)||length!=expected||
       memcmp(bytes,"THR1",4)!=0||get32(bytes+4)!=1||get32(bytes+8)!=THERMAL_HEADER_BYTES||
       get32(bytes+16)!=count||get32(bytes+20)!=bits(FR_THERMAL_AMBIENT_K)||
       get32(bytes+24)!=bits(fr_reactive_profile(DC_MATERIAL_WATER)->heat_capacity)||get32(bytes+28)!=FR_PROFILE_ABI||
       get64(bytes+40)!=checksum(bytes,length)||(count&&!chunks))
        return error(err,cap,"Invalid, incompatible or corrupt thermal checkpoint framing");
    fr_thermal_checkpoint_t value={.source={number(get32(bytes+32)),number(get32(bytes+36))},.purpose=get32(bytes+12),.chunk_count=count};
    value.water_excess=count?malloc((size_t)count*DC_CHUNK_CELLS*sizeof(float)):NULL;
    if(count&&!value.water_excess) return error(err,cap,"Cannot allocate thermal checkpoint inventory");
    bool okay=true;
    for(uint32_t c=0;okay&&c<count;c++) {
        const uint8_t *record=bytes+THERMAL_HEADER_BYTES+(size_t)c*THERMAL_RECORD_BYTES;
        okay=get64(record)==(uint64_t)chunks[c].coord.x&&get64(record+8)==(uint64_t)chunks[c].coord.y;
        for(uint32_t i=0;i<DC_CHUNK_CELLS;i++) value.water_excess[(size_t)c*DC_CHUNK_CELLS+i]=number(get32(record+16+i*4u));
    }
    if(!okay) {fr_thermal_checkpoint_release(&value);return error(err,cap,"Thermal checkpoint does not match physical liquid inventory");}
    if(!valid(&value,chunks,count,err,cap)) {fr_thermal_checkpoint_release(&value);return false;}
    *out=value;return true;
}
void fr_thermal_checkpoint_release(fr_thermal_checkpoint_t *state) {
    if(state) {free(state->water_excess);memset(state,0,sizeof(*state));}
}
