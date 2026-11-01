#include <math.h>
#include <stdio.h>
#include <string.h>

#include "beaver_internal.h"

enum { BEAVER_PROP_HEADER=64, BEAVER_PROP_FRAME=16, BEAVER_PROP_MAP=8,
       BEAVER_PROP_APPEARANCE=64, BEAVER_PROP_STONE=1, BEAVER_PROP_WOOD=2,
       BEAVER_PROP_RIGID=3 };

static uint32_t beaver_prop_material(fr_beaver_region_t region) {
    return region==FR_BEAVER_TEETH?BEAVER_PROP_STONE:BEAVER_PROP_WOOD;
}

static void beaver_u32(uint8_t *bytes,uint32_t value) {
    for (uint32_t i=0;i<4;i++) bytes[i]=(uint8_t)(value>>(i*8));
}

static void beaver_float(uint8_t *bytes,float value) {
    uint32_t bits;memcpy(&bits,&value,sizeof(bits));beaver_u32(bytes,bits);
}

static uint32_t beaver_crc(const uint8_t *bytes,size_t size) {
    uint32_t crc=UINT32_MAX;
    for (size_t i=0;i<size;i++) {
        if (i>=44 && i<48) continue;
        crc^=bytes[i];
        for (uint32_t bit=0;bit<8;bit++) crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));
    }
    return ~crc;
}

static bool beaver_error(char *err,uint32_t cap,const char *message) {
    if (err && cap) snprintf(err,cap,"%s",message);
    return false;
}

bool fr_beaver_asset_size(fr_player_action_t action,size_t *bytes,uint32_t *entries) {
    fr_player_clip_t clip;
    if (!bytes || !entries || !fr_player_clip(action,&clip)) return false;
    *entries=clip.frames*FR_BEAVER_PIXELS;
    *bytes=BEAVER_PROP_HEADER+(size_t)clip.frames*BEAVER_PROP_FRAME+
        (size_t)*entries*(BEAVER_PROP_MAP+BEAVER_PROP_APPEARANCE);
    return true;
}

bool fr_beaver_source(uint32_t source,fr_beaver_source_t *out) {
    beaver_art_pixel_t art;
    if (!out || !beaver_art_source(source,&art)) return false;
    *out=(fr_beaver_source_t){art.region,beaver_prop_material(art.region)};
    return true;
}

static void beaver_frame(uint8_t *data,uint32_t *mapping,fr_player_action_t action,uint32_t frame) {
    for (uint32_t original=0;original<FR_BEAVER_PIXELS;original++) {
        beaver_art_pixel_t art;
        uint32_t source;
        if (!beaver_art_frame(action==FR_PLAYER_CHEW?frame:0,original,&art,&source)) continue;
        int32_t x=(int32_t)(original%FR_BEAVER_WIDTH),y=(int32_t)(original/FR_BEAVER_WIDTH);
        beaver_art_pose(&art,action,frame,&x,&y);
        if (x<0 || y<0 || x>=(int32_t)FR_BEAVER_WIDTH || y>=(int32_t)FR_BEAVER_HEIGHT) continue;
        uint32_t pixel=(uint32_t)y*FR_BEAVER_WIDTH+(uint32_t)x;
        uint8_t *physical=data+pixel*BEAVER_PROP_MAP;
        physical[0]=(uint8_t)beaver_prop_material(art.region);
        physical[2]=BEAVER_PROP_RIGID;
        beaver_u32(physical+4,1);
        uint8_t *appearance=data+FR_BEAVER_PIXELS*BEAVER_PROP_MAP+pixel*BEAVER_PROP_APPEARANCE;
        for (uint32_t c=0;c<3;c++) {
            float srgb=art.color[c]/255.f;
            float linear=srgb<=.04045f?srgb/12.92f:powf((srgb+.055f)/1.055f,2.4f);
            beaver_float(appearance+c*4,linear);
        }
        beaver_float(appearance+12,.8f);
        beaver_float(appearance+24,1.f);
        beaver_float(appearance+48,1.f);
        mapping[pixel]=source;
    }
}

bool fr_beaver_asset_build(fr_player_action_t action,uint8_t *bytes,size_t capacity,
    uint32_t *mapping,uint32_t mapping_capacity,fr_beaver_asset_t *out,char *err,uint32_t cap) {
    size_t size;uint32_t entries;fr_player_clip_t clip;
    if (!out || !bytes || !mapping || !fr_beaver_asset_size(action,&size,&entries) ||
        !fr_player_clip(action,&clip) || capacity<size || mapping_capacity<entries)
        return beaver_error(err,cap,"Invalid beaver clip or insufficient caller storage");
    memset(bytes,0,size);memset(mapping,0,(size_t)entries*sizeof(*mapping));
    memcpy(bytes,"CPRP",4);beaver_u32(bytes+4,1);beaver_u32(bytes+8,(uint32_t)size);
    beaver_u32(bytes+12,FR_BEAVER_WIDTH);beaver_u32(bytes+16,FR_BEAVER_HEIGHT);
    beaver_u32(bytes+20,clip.frames);beaver_u32(bytes+24,1);
    beaver_u32(bytes+28,BEAVER_PROP_HEADER);
    uint32_t data_offset=BEAVER_PROP_HEADER+clip.frames*BEAVER_PROP_FRAME;
    beaver_u32(bytes+32,data_offset);beaver_u32(bytes+36,BEAVER_PROP_MAP);
    beaver_u32(bytes+40,BEAVER_PROP_APPEARANCE);memcpy(bytes+48,"beaver-sprite-v1",16);
    bytes[63]=(uint8_t)action;
    uint32_t frame_size=FR_BEAVER_PIXELS*(BEAVER_PROP_MAP+BEAVER_PROP_APPEARANCE);
    for (uint32_t f=0;f<clip.frames;f++) {
        uint8_t *table=bytes+BEAVER_PROP_HEADER+f*BEAVER_PROP_FRAME;
        beaver_u32(table,clip.frame_ms);beaver_u32(table+4,f*frame_size);
        beaver_u32(table+8,FR_BEAVER_PIXELS*BEAVER_PROP_MAP);
        beaver_u32(table+12,FR_BEAVER_PIXELS*BEAVER_PROP_APPEARANCE);
        beaver_frame(bytes+data_offset+f*frame_size,mapping+f*FR_BEAVER_PIXELS,action,f);
    }
    beaver_u32(bytes+44,beaver_crc(bytes,size));
    fr_beaver_asset_t asset={.source_pixels=mapping,.source_count=entries,.action=action};
    if (!fr_prop_asset_open(&asset.prop,bytes,size,err,cap)) return false;
    *out=asset;
    return true;
}

bool fr_beaver_asset_pixel(const fr_beaver_asset_t *asset,uint32_t frame,
    uint32_t x,uint32_t y,bool flip,fr_prop_pixel_t *out,uint32_t *source) {
    if (!asset || !asset->source_pixels || !out || !source ||
        asset->prop.width!=FR_BEAVER_WIDTH || asset->prop.height!=FR_BEAVER_HEIGHT ||
        frame>=asset->prop.frame_count || x>=FR_BEAVER_WIDTH || y>=FR_BEAVER_HEIGHT ||
        (uint64_t)asset->prop.frame_count*FR_BEAVER_PIXELS!=asset->source_count) return false;
    if (flip) x=FR_BEAVER_WIDTH-1-x;
    uint32_t pixel=y*FR_BEAVER_WIDTH+x;
    fr_prop_pixel_t physical;
    if (!fr_prop_pixel(&asset->prop,frame,pixel,&physical)) return false;
    uint32_t rest=asset->source_pixels[frame*FR_BEAVER_PIXELS+pixel];
    fr_beaver_source_t metadata;
    if (physical.material?( !fr_beaver_source(rest,&metadata) || metadata.prop_material!=physical.material):rest!=0) return false;
    *out=physical;*source=rest;
    return true;
}

bool fr_beaver_pixel_owners(uint32_t source,uint64_t identity,uint32_t body_id,uint32_t generation,
    fr_appearance_owner_t *appearance,fr_reactive_owner_t *reactive) {
    fr_beaver_source_t metadata;
    if (!identity || !body_id || !generation || !appearance || !reactive ||
        !fr_beaver_source(source,&metadata)) return false;
    uint32_t x=(source-1)%FR_BEAVER_WIDTH,y=(source-1)/FR_BEAVER_WIDTH;
    *appearance=(fr_appearance_owner_t){.identity=identity,.generation=generation,.x=x,.y=y,
        .pixel_index=source-1,.material=fr_prop_body_material(metadata.prop_material),
        .domain=FR_APPEARANCE_BODY,.visual_scale=1};
    *reactive=(fr_reactive_owner_t){.kind=FR_REACTIVE_OWNER_BODY,.key={body_id,generation,x,y,0}};
    return true;
}
