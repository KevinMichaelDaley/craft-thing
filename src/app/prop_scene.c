#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <SDL.h>

#include "prop_scene.h"
#include "view_config.h"

static uint32_t little_word(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}

dc_level_view_t *dc_prop_scene_create(const char *directory, uint64_t seed,
                                      const char *path, char *err, uint32_t cap) {
    const char *slash=strrchr(path,'/');
    char companion[1024];
    int length=slash?snprintf(companion,sizeof(companion),"%.*s/ground.bin",(int)(slash-path),path):
        snprintf(companion,sizeof(companion),"ground.bin");
    if (length<0 || (size_t)length>=sizeof(companion)) {snprintf(err,cap,"Scene path too long");return NULL;}
    FILE *file=fopen(companion,"rb");
    if (!file && errno==ENOENT) return dc_level_view_create(directory,seed,err,cap);
    if (!file) {snprintf(err,cap,"Cannot read scene ground companion");return NULL;}
    uint8_t header[16];
    bool okay=fread(header,1,sizeof(header),file)==sizeof(header) && !memcmp(header,"GND1",4);
    uint32_t width=okay?little_word(header+4):0,height=okay?little_word(header+8):0;
    uint32_t depth=okay?little_word(header+12):0;
    okay=okay && width>0 && width<=4194304u && height>0 && height<=4194304u && depth>0 && depth<=height;
    uint32_t *surface=okay?malloc((size_t)width*4):NULL;
    okay=surface && fread(surface,4,width,file)==width && fgetc(file)==EOF;
    fclose(file);
    for (uint32_t i=0;i<width && okay;i++) {
        surface[i]=little_word((uint8_t*)surface+i*4);
        okay=surface[i]<height;
    }
    if (okay) {
        FILE *source=fopen(path,"rb");
        uint8_t asset_header[20];
        okay=source && fread(asset_header,1,sizeof(asset_header),source)==sizeof(asset_header) &&
            !memcmp(asset_header,"CPRP",4) && little_word(asset_header+12)==width && little_word(asset_header+16)==height;
        if (source) fclose(source);
    }
    if (!okay) {free(surface);snprintf(err,cap,"Invalid bounded ground surface companion");return NULL;}
    dc_ground_profile_t ground={32-(int64_t)width/2,32-(int64_t)height/2,width,depth,surface};
    dc_level_view_t *view=dc_level_view_create_ground(directory,seed,&ground,err,cap);
    free(surface);return view;
}

bool dc_prop_scene_load(dc_level_view_t *view, const char *path, char *err, uint32_t cap) {
    FILE *file=fopen(path,"rb");
    if (!file) {snprintf(err,cap,"Cannot open scene %s",path);return false;}
    bool okay=fseek(file,0,SEEK_END)==0;
    long length=okay?ftell(file):-1;
    okay=length>0 && length<=256*1024*1024 && fseek(file,0,SEEK_SET)==0;
    uint8_t *bytes=okay?malloc((size_t)length):NULL;
    okay=bytes && fread(bytes,1,(size_t)length,file)==(size_t)length;
    fclose(file);
    if (!okay) {free(bytes);snprintf(err,cap,"Cannot preload bounded scene file");return false;}
    fr_prop_asset_t asset;
    okay=fr_prop_asset_open(&asset,bytes,(size_t)length,err,cap);
    if (okay) {
        dc_chunk_coord_t chunk;
        uint32_t local_x,local_y;
        dc_cell_address(32-(int64_t)asset.width/2,32-(int64_t)asset.height/2,&chunk,&local_x,&local_y);
        okay=dc_level_view_set_spring_enabled(view,false) &&
            dc_level_view_pan_pixels(view,32-VIEW_WIDTH/2,32-INITIAL_CHUNK_Y*64-VIEW_HEIGHT/2) &&
            dc_level_view_wait_visible(view,120000,err,cap) &&
            (dc_level_view_restored(view)?dc_level_view_restore_prop_bonds(view,&asset,chunk,local_x,local_y,err,cap):
                dc_level_view_place_prop(view,&asset,chunk,local_x,local_y,err,cap)) &&
            dc_level_view_renderer_mode(view,FR_RENDERER_ALBEDO) &&
            dc_level_view_tick(view,err,cap);
        if (okay) printf("Scene %s: %ux%u centered on chunk(0,0), world top-left(%lld,%lld), terrain and authored color loaded\n",
            path,asset.width,asset.height,(long long)(32-(int64_t)asset.width/2),(long long)(32-(int64_t)asset.height/2));
    }
    free(bytes);return okay;
}

static bool capture(dc_level_view_t *view, uint32_t *pixels, const char *path, char *err, uint32_t cap) {
    if (!dc_level_view_pixels(view,pixels,VIEW_WIDTH*VIEW_HEIGHT,err,cap)) return false;
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,VIEW_WIDTH,VIEW_HEIGHT,32,VIEW_WIDTH*4,SDL_PIXELFORMAT_ABGR8888);
    bool okay=surface && SDL_SaveBMP(surface,path)==0;
    if (surface) SDL_FreeSurface(surface);
    return okay;
}

int dc_prop_scene_smoke(const char *path) {
    char directory[]="build/ui_prop_scene_XXXXXX",err[256]={0};
    if (!mkdtemp(directory)) return 1;
    dc_level_view_t *view=dc_prop_scene_create(directory,314,path,err,sizeof(err));
    uint32_t *before=malloc((size_t)VIEW_WIDTH*VIEW_HEIGHT*4);
    uint32_t *after=malloc((size_t)VIEW_WIDTH*VIEW_HEIGHT*4);
    dc_chunk_t *chunk=calloc(1,sizeof(*chunk));
    mkdir("build/screenshots",0777);
    bool okay=view && before && after && chunk && dc_prop_scene_load(view,path,err,sizeof(err)) &&
        dc_level_view_chunk(view,(dc_chunk_coord_t){0,0},chunk,err,sizeof(err));
    uint32_t stone=0,wood=0;
    if (okay) for (uint32_t i=0;i<DC_CHUNK_CELLS;i++) {
        stone+=chunk->cells[i].material==DC_MATERIAL_STONE;
        wood+=chunk->cells[i].material==DC_MATERIAL_WOOD;
    }
    okay=okay && stone>0 && wood>0 && capture(view,before,"build/screenshots/level_piece01_placed.bmp",err,sizeof(err));
    if (okay) {
        okay=dc_level_view_chunk(view,(dc_chunk_coord_t){-1,0},chunk,err,sizeof(err)) &&
            chunk->cells[43*64+31].material==DC_MATERIAL_AIR &&
            chunk->cells[44*64+31].material==DC_MATERIAL_DIRT &&
            dc_level_view_chunk(view,(dc_chunk_coord_t){1,0},chunk,err,sizeof(err)) &&
            chunk->cells[34*64+32].material==DC_MATERIAL_AIR &&
            chunk->cells[35*64+32].material==DC_MATERIAL_DIRT;
    }
    for (uint32_t i=0;i<4 && okay;i++)
        okay=dc_level_view_tick(view,err,sizeof(err)) &&
            dc_level_view_pixels(view,after,VIEW_WIDTH*VIEW_HEIGHT,err,sizeof(err)) &&
            memcmp(before,after,(size_t)VIEW_WIDTH*VIEW_HEIGHT*4)==0;
    okay=okay && dc_level_view_paint(view,VIEW_WIDTH/2,VIEW_HEIGHT/2-40,2,DC_MATERIAL_STONE,err,sizeof(err)) &&
        dc_level_view_tick(view,err,sizeof(err)) &&
        capture(view,after,"build/screenshots/level_piece01_painted.bmp",err,sizeof(err)) &&
        memcmp(before,after,(size_t)VIEW_WIDTH*VIEW_HEIGHT*4)!=0;
    for (uint32_t i=0;i<12 && okay;i++)
        okay=dc_level_view_step(view,err,sizeof(err)) && dc_level_view_tick(view,err,sizeof(err));
    okay=okay && capture(view,after,"build/screenshots/level_piece01_physics.bmp",err,sizeof(err));
    if (view && !dc_level_view_destroy(view,err,sizeof(err))) okay=false;
    printf("Scene placement/central chunk/paused frames/brush/12 physics ticks: %s (central stone %u wood %u) %s\n",
           okay?"PASS":"FAIL",stone,wood,err);
    free(chunk);free(before);free(after);return okay?0:1;
}
